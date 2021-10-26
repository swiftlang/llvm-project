//===- PrefixMapper.cpp - Prefix mapping utility --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/PrefixMapper.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/Support/VirtualFileSystem.h"

using namespace llvm;

Optional<MappedPrefix> MappedPrefix::getFromJoined(StringRef JoinedMapping) {
  auto Equals = JoinedMapping.find('=');
  if (Equals == StringRef::npos)
    return None;
  StringRef Old = JoinedMapping.substr(0, Equals);
  StringRef New = JoinedMapping.substr(Equals + 1);
  return MappedPrefix{Old, New};
}

template <bool StopOnInvalid, class StringT>
static Optional<StringRef>
transformJoinedImpl(ArrayRef<StringT> JoinedMappings,
                    SmallVectorImpl<MappedPrefix> &Mappings) {
  size_t OriginalSize = Mappings.size();
  for (StringRef Joined : JoinedMappings) {
    if (Optional<MappedPrefix> Split = MappedPrefix::getFromJoined(Joined)) {
      Mappings.push_back(*Split);
      continue;
    }
    if (!StopOnInvalid)
      continue;
    Mappings.resize(OriginalSize);
    return Joined;
  }
  return None;
}

static Error makeErrorForInvalidJoin(Optional<StringRef> Joined) {
  if (!Joined)
    return Error::success();
  return createStringError(inconvertibleErrorCode(),
                           "invalid prefix map: '" + *Joined + "'");
}

Error MappedPrefix::transformJoined(ArrayRef<StringRef> Joined,
                                    SmallVectorImpl<MappedPrefix> &Split) {
  return makeErrorForInvalidJoin(
      transformJoinedImpl</*StopOnInvalid*/ true>(Joined, Split));
}

Error MappedPrefix::transformJoined(ArrayRef<std::string> Joined,
                                    SmallVectorImpl<MappedPrefix> &Split) {
  return makeErrorForInvalidJoin(
      transformJoinedImpl</*StopOnInvalid*/ true>(Joined, Split));
}

void MappedPrefix::transformJoinedIfValid(
    ArrayRef<StringRef> Joined, SmallVectorImpl<MappedPrefix> &Split) {
  transformJoinedImpl</*StopOnInvalid*/ false>(Joined, Split);
}

void MappedPrefix::transformJoinedIfValid(
    ArrayRef<std::string> Joined, SmallVectorImpl<MappedPrefix> &Split) {
  transformJoinedImpl</*StopOnInvalid*/ false>(Joined, Split);
}

/// FIXME: Copy/pasted from llvm/lib/Support/Path.cpp.
static bool startsWith(StringRef Path, StringRef Prefix,
                       sys::path::Style PathStyle) {
  if (PathStyle == sys::path::Style::posix ||
      (PathStyle == sys::path::Style::native &&
       sys::path::system_style() == sys::path::Style::posix))
    return Path.startswith(Prefix);

  if (Path.size() < Prefix.size())
    return false;

  // Windows prefix matching : case and separator insensitive
  for (size_t I = 0, E = Prefix.size(); I != E; ++I) {
    bool SepPath = sys::path::is_separator(Path[I], PathStyle);
    bool SepPrefix = sys::path::is_separator(Prefix[I], PathStyle);
    if (SepPath != SepPrefix)
      return false;
    if (SepPath)
      continue;
    if (toLower(Path[I]) != toLower(Prefix[I]))
      return false;
  }
  return true;
}

Optional<StringRef> PrefixMapper::mapImpl(StringRef Path,
                                          SmallVectorImpl<char> &Storage) {
  for (const MappedPrefix &Map : Mappings) {
    StringRef Old = Map.Old;
    StringRef New = Map.New;
    if (!startsWith(Path, Old, PathStyle))
      continue;
    StringRef Suffix = Path.drop_front(Old.size());
    if (Suffix.empty())
      return New; // Exact match.

    // Don't remap "/old-suffix" with mapping "/old=/new".
    if (!llvm::sys::path::is_separator(Suffix.front(), PathStyle))
      continue;

    // Drop the separator, append, and return.
    Storage.assign(New.begin(), New.end());
    llvm::sys::path::append(Storage, PathStyle, Suffix.drop_front());
    return StringRef(Storage.begin(), Storage.size());
  }
  return None;
}

void PrefixMapper::map(StringRef Path, SmallVectorImpl<char> &NewPath) {
  NewPath.clear();
  Optional<StringRef> Mapped = mapImpl(Path, NewPath);
  if (!NewPath.empty())
    return;
  if (!Mapped)
    Mapped = Path;
  NewPath.assign(Mapped->begin(), Mapped->end());
}

void PrefixMapper::map(StringRef Path, std::string &NewPath) {
  NewPath = mapToString(Path);
}

StringRef PrefixMapper::map(StringRef Path) {
  SmallString<256> Storage;
  Optional<StringRef> Mapped = mapImpl(Path, Storage);
  if (!Mapped)
    return Path;
  if (Storage.empty())
    return *Mapped; // Exact match.
  return Saver.save(StringRef(Storage));
}

std::string PrefixMapper::mapToString(StringRef Path) {
  SmallString<256> Storage;
  Optional<StringRef> Mapped = mapImpl(Path, Storage);
  return Mapped ? Mapped->str() : Path.str();
}

void PrefixMapper::mapInPlace(SmallVectorImpl<char> &Path) {
  SmallString<256> Storage;
  Optional<StringRef> Mapped =
      mapImpl(StringRef(Path.begin(), Path.size()), Storage);
  if (!Mapped)
    return;
  if (Storage.empty())
    Path.assign(Mapped->begin(), Mapped->end());
  else
    Storage.swap(Path);
}

void PrefixMapper::mapInPlace(std::string &Path) {
  SmallString<256> Storage;
  Optional<StringRef> Mapped = mapImpl(Path, Storage);
  if (!Mapped)
    return;
  Path.assign(Mapped->begin(), Mapped->size());
}

void PrefixMapper::sort() {
  // FIXME: Only works for posix right now since it doesn't handle case- and
  // separator-insensitivity.
  std::stable_sort(Mappings.begin(), Mappings.end(),
                   [](const MappedPrefix &LHS, const MappedPrefix &RHS) {
                     return LHS.Old > RHS.Old;
                   });
}

RealPathPrefixMapper::RealPathPrefixMapper(
    IntrusiveRefCntPtr<vfs::FileSystem> FS, StringSaver &Saver,
    sys::path::Style PathStyle)
    : PM(Saver, PathStyle), FS(std::move(FS)) {}

RealPathPrefixMapper::~RealPathPrefixMapper() = default;

Error RealPathPrefixMapper::getRealPath(StringRef Path,
                                        SmallVectorImpl<char> &RealPath) {
  assert(RealPath.empty() && "Expected to be fed an empty RealPath");
  if (Path.empty())
    return Error::success();
  if (std::error_code EC = FS->getRealPath(Path, RealPath))
    return createFileError(Path, EC);
  return Error::success();
}

Error RealPathPrefixMapper::map(StringRef Path,
                                SmallVectorImpl<char> &NewPath) {
  NewPath.clear();
  if (Error E = getRealPath(Path, NewPath))
    return E;
  PM.mapInPlace(NewPath);
  return Error::success();
}

Expected<StringRef> RealPathPrefixMapper::map(StringRef Path) {
  SmallString<256> RealPath;
  if (Error E = getRealPath(Path, RealPath))
    return std::move(E);
  return PM.map(RealPath);
}

Expected<std::string> RealPathPrefixMapper::mapToString(StringRef Path) {
  SmallString<256> RealPath;
  if (Error E = getRealPath(Path, RealPath))
    return std::move(E);
  return PM.mapToString(RealPath);
}

Error RealPathPrefixMapper::mapInPlace(SmallVectorImpl<char> &Path) {
  SmallString<256> RealPath;
  if (Error E = getRealPath(StringRef(Path.begin(), Path.size()), RealPath))
    return E;
  PM.map(RealPath, Path);
  return Error::success();
}

Error RealPathPrefixMapper::mapInPlace(std::string &Path) {
  SmallString<256> RealPath;
  if (Error E = getRealPath(Path, RealPath))
    return E;
  Path = PM.mapToString(RealPath);
  return Error::success();
}

Error RealPathPrefixMapper::makePrefixReal(StringRef &Prefix) {
  SmallString<256> RealPath;
  if (Error E = getRealPath(Prefix, RealPath))
    return E;
  if (RealPath != Prefix)
    Prefix = PM.getStringSaver().save(StringRef(RealPath));
  return Error::success();
}

Error RealPathPrefixMapper::add(const MappedPrefix &Mapping) {
  StringRef Old = Mapping.Old;
  StringRef New = Mapping.New;
  if (Error E = makePrefixReal(Old))
    return E;
  PM.add(MappedPrefix{Old, New});
  return Error::success();
}
