//===- llvm/Support/PrefixMapper.h - Prefix mapping utility -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_TABLEGEN_PREFIXMAPPER_H
#define LLVM_TABLEGEN_PREFIXMAPPER_H

#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/IntrusiveRefCntPtr.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"

namespace llvm {

namespace vfs {
class FileSystem;
} // end namespace vfs

struct MappedPrefix {
  StringRef Old;
  StringRef New;

  MappedPrefix getInverse() const { return MappedPrefix{New, Old}; }

  bool operator==(const MappedPrefix &RHS) const {
    return Old == RHS.Old && New == RHS.New;
  }
  bool operator!=(const MappedPrefix &RHS) const { return !(*this == RHS); }

  static Optional<MappedPrefix> getFromJoined(StringRef JoinedMapping);

  static Error transformJoined(ArrayRef<StringRef> Joined,
                               SmallVectorImpl<MappedPrefix> &Split);
  static Error transformJoined(ArrayRef<std::string> Joined,
                               SmallVectorImpl<MappedPrefix> &Split);
  static void transformJoinedIfValid(ArrayRef<StringRef> Joined,
                                     SmallVectorImpl<MappedPrefix> &Split);
  static void transformJoinedIfValid(ArrayRef<std::string> Joined,
                                     SmallVectorImpl<MappedPrefix> &Split);
};

/// Remap path prefixes.
///
/// FIXME: The StringSaver should be optional. Only APIs returning StringRef
/// need it, and those could assert/crash if one is not configured.
class PrefixMapper {
public:
  /// Map \p Path, and saving the new (or existing) path in \p NewPath.
  ///
  /// \pre \p Path is not a reference into \p NewPath.
  void map(StringRef Path, SmallVectorImpl<char> &NewPath);
  void map(StringRef Path, std::string &NewPath);

  /// Map \p Path, returning \a StringSaver::save() for new paths that aren't
  /// exact matches.
  StringRef map(StringRef Path);

  /// Map \p Path, returning \a std::string.
  std::string mapToString(StringRef Path);

  /// Map \p Path in place.
  void mapInPlace(SmallVectorImpl<char> &Path);
  void mapInPlace(std::string &Path);

private:
  /// Map (or unmap) \p Path. On a match, fills \p Storage with the mapped path
  /// unless it's an exact match.
  ///
  /// \pre \p Path is not a reference into \p Storage.
  Optional<StringRef> mapImpl(StringRef Path, SmallVectorImpl<char> &Storage);

public:
  void add(const MappedPrefix &MP) { Mappings.push_back(MP); }

  /// A path-based reverse lexicographic sort, putting deeper paths first so
  /// that deeper paths are prioritized over their parent paths. For example,
  /// if both the source and build directories are remapped and one is nested
  /// inside the other, the nested one will come first.
  ///
  /// FIXME: Doubtful that this would work correctly on windows, since it's
  /// case- and separator-sensitive.
  ///
  /// FIXME: Should probably be done implicitly, maybe by moving to a std::set
  /// or std::map.
  ///
  /// TODO: Test.
  void sort();

  template <class RangeT> void addRange(const RangeT &Mappings) {
    this->Mappings.append(Mappings.begin(), Mappings.end());
  }

  template <class RangeT> void addInverseRange(const RangeT &Mappings) {
    for (const MappedPrefix &M : Mappings)
      add(M.getInverse());
  }

  ArrayRef<MappedPrefix> getMappings() const { return Mappings; }

  StringSaver &getStringSaver() { return Saver; }
  sys::path::Style getPathStyle() const { return PathStyle; }

  PrefixMapper(StringSaver &Saver,
               sys::path::Style PathStyle = sys::path::Style::native)
      : PathStyle(PathStyle), Saver(Saver) {}

private:
  sys::path::Style PathStyle;
  StringSaver &Saver;
  SmallVector<MappedPrefix> Mappings;
};

/// Wrapper for \a PrefixMapper that maps using the result of \a
/// FileSystem::getRealPath(). Returns an error if inputs cannot be found.
///
/// FIXME: The StringSaver should be optional. Only APIs returning StringRef
/// need it, and those could assert/crash if one is not configured.
class RealPathPrefixMapper {
public:
  Error map(StringRef Path, SmallVectorImpl<char> &NewPath);
  Error map(StringRef Path, std::string &NewPath) {
    return mapToString(Path).moveInto(NewPath);
  }
  Expected<StringRef> map(StringRef Path);
  Expected<std::string> mapToString(StringRef Path);
  Error mapInPlace(SmallVectorImpl<char> &Path);
  Error mapInPlace(std::string &Path);

  void mapOrOriginal(StringRef Path, SmallVectorImpl<char> &NewPath) {
    if (errorToBool(map(Path, NewPath)))
      NewPath.assign(Path.begin(), Path.end());
  }
  void mapOrOriginal(StringRef Path, std::string &NewPath) {
    if (errorToBool(map(Path, NewPath)))
      NewPath.assign(Path.begin(), Path.end());
  }
  Optional<StringRef> mapOrNone(StringRef Path) {
    return expectedToOptional(map(Path));
  }
  StringRef mapOrOriginal(StringRef Path) {
    Optional<StringRef> Mapped = mapOrNone(Path);
    return Mapped ? *Mapped : Path;
  }
  Optional<std::string> mapToStringOrNone(StringRef Path) {
    return expectedToOptional(mapToString(Path));
  }
  void mapInPlaceOrClear(SmallVectorImpl<char> &Path) {
    if (errorToBool(mapInPlace(Path)))
      Path.clear();
  }
  void mapInPlaceOrClear(std::string &Path) {
    if (errorToBool(mapInPlace(Path)))
      Path.clear();
  }

private:
  Error getRealPath(StringRef Path, SmallVectorImpl<char> &RealPath);
  Error makePrefixReal(StringRef &Prefix);

public:
  ArrayRef<MappedPrefix> getMappings() const { return PM.getMappings(); }
  sys::path::Style getPathStyle() const { return PM.getPathStyle(); }

  Error add(const MappedPrefix &Mapping);

  template <class RangeT> Error addRange(const RangeT &Mappings) {
    for (const MappedPrefix &M : Mappings)
      if (Error E = add(M))
        return E;
    return Error::success();
  }

  template <class RangeT> Error addInverseRange(const RangeT &Mappings) {
    for (const MappedPrefix &M : Mappings)
      if (Error E = add(M.getInverse()))
        return E;
    return Error::success();
  }

  template <class RangeT> void addRangeIfValid(const RangeT &Mappings) {
    for (const MappedPrefix &M : Mappings)
      consumeError(add(M));
  }

  template <class RangeT> void addInverseRangeIfValid(const RangeT &Mappings) {
    for (const MappedPrefix &M : Mappings)
      consumeError(add(M.getInverse()));
  }

  void sort() { PM.sort(); }

  RealPathPrefixMapper(IntrusiveRefCntPtr<vfs::FileSystem> FS,
                       StringSaver &Saver,
                       sys::path::Style PathStyle = sys::path::Style::native);
  ~RealPathPrefixMapper();

private:
  PrefixMapper PM;
  IntrusiveRefCntPtr<vfs::FileSystem> FS;
};

} // end namespace llvm

#endif // LLVM_TABLEGEN_PREFIXMAPPER_H
