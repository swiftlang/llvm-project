//===- ScanDependencies.cpp - TableGen scan-deps implementation -----------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/TableGen/ScanDependencies.h"
#include "llvm/CAS/CachingOnDiskFileSystem.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/PrefixMapper.h"

using namespace llvm;
using namespace llvm::tablegen;

void tablegen::flattenStrings(ArrayRef<StringRef> List,
                              SmallVectorImpl<char> &Flattened) {
  for (StringRef Name : List) {
    Flattened.append(Name.begin(), Name.end());
    Flattened.push_back(0);
  }
}

void tablegen::splitFlattenedStrings(StringRef Flattened,
                                     SmallVectorImpl<StringRef> &List) {
  while (!Flattened.empty()) {
    auto Null = Flattened.find(0);
    // FIXME: Return an error, or bail out and try uncached?
    assert(Null != StringRef::npos &&
           "Expected valid list of null-terminated includes");
    List.push_back(Flattened.take_front(Null));
    Flattened = Flattened.drop_front(Null + 1);
  }
}

void tablegen::scanTextForIncludes(StringRef Input,
                                   SmallVectorImpl<StringRef> &Includes) {
  constexpr StringLiteral Keyword = "include";
  SmallString<256> ResultToCache;
  for (auto I = Input.find(Keyword); I != StringRef::npos;
       I = Input.find(Keyword)) {
    // Check (non-exhaustively!) if we can skip this match because it's part of
    // a longer identifier.
    bool Skip = I > 0 && (isalpha(Input[I - 1]) || isdigit(Input[I - 1]));
    Input = Input.drop_front(I + Keyword.size());
    if (Input.size() < 3 || Skip)
      return;
    auto Pos = Input.find_first_not_of(" \t\v\r\n");
    if (Pos == StringRef::npos)
      return;
    {
      auto Q = Input[Pos];
      Input = Input.drop_front(Pos + 1);
      if (Q != '"')
        continue;
    }

    // Assume that tablegen files don't have escaped quotes in their
    // filenames.
    Pos = Input.find_first_of("\r\n\"");
    if (Pos == StringRef::npos)
      return;
    StringRef Name = Input.take_front(Pos);
    {
      auto Q = Input[Pos];
      Input = Input.drop_front(Pos + 1);
      if (Q != '"')
        continue;
    }

    Includes.push_back(Name);
  }
}

static Error
fetchCachedIncludedFiles(cas::CASDB &CAS, cas::CASID ID,
                         SmallVectorImpl<StringRef> &IncludedFiles) {
  Expected<cas::BlobRef> ExpectedBlob = CAS.getBlob(ID);
  if (!ExpectedBlob)
    return ExpectedBlob.takeError();
  splitFlattenedStrings(ExpectedBlob->getData(), IncludedFiles);
  return Error::success();
}

static Error computeIncludedFiles(cas::CASDB &CAS, cas::CASID Key,
                                  StringRef Input,
                                  SmallVectorImpl<StringRef> &IncludedFiles) {
  SmallString<256> ResultToCache;
  scanTextForIncludes(Input, IncludedFiles);
  flattenStrings(IncludedFiles, ResultToCache);

  Expected<cas::BlobRef> ExpectedResult = CAS.createBlob(ResultToCache);
  if (!ExpectedResult)
    return ExpectedResult.takeError();
  if (Error E = CAS.putCachedResult(Key, *ExpectedResult))
    return E;
  return Error::success();
}

Error tablegen::scanTextForIncludes(cas::CASDB &CAS, cas::CASID ExecID,
                                    const cas::BlobRef &Blob,
                                    SmallVectorImpl<StringRef> &Includes) {
  constexpr StringLiteral CacheKeyData = "llvm::tablegen::scanTextForIncludes";

  Expected<cas::NodeRef> Key = CAS.createNode({ExecID, Blob}, CacheKeyData);
  if (!Key)
    return Key.takeError();

  if (Optional<cas::CASID> ResultID =
          expectedToOptional(CAS.getCachedResult(*Key)))
    return fetchCachedIncludedFiles(CAS, *ResultID, Includes);
  return computeIncludedFiles(CAS, *Key, Blob.getData(), Includes);
}

Optional<cas::CASID>
tablegen::lookupIncludeID(cas::CachingOnDiskFileSystem &FS,
                          ArrayRef<std::string> IncludeDirs,
                          StringRef Filename) {
  SmallString<256> Path;
  Optional<cas::CASID> ID;
  if (!FS.statusAndFileID(Filename, ID).getError())
    return ID;
  for (StringRef Dir : IncludeDirs) {
    // Match path logic from SourceMgr::AddIncludeFile.
    Path.assign(Dir);
    Path.append(sys::path::get_separator());
    Path.append(Filename);
    if (!FS.statusAndFileID(Path, ID).getError())
      return ID;
  }
  return ID;
}

Error tablegen::accessAllIncludes(cas::CachingOnDiskFileSystem &FS,
                                  cas::CASID ExecID,
                                  ArrayRef<std::string> IncludeDirs,
                                  const cas::BlobRef &MainFileBlob) {
  cas::CASDB &CAS = FS.getCAS();

  // Helper for adding to the worklist.
  SmallVector<cas::BlobRef> Worklist = {MainFileBlob};
  SmallDenseSet<cas::CASID, 16> Seen;
  Seen.insert(MainFileBlob);
  auto push = [&Seen, &Worklist, &CAS](cas::CASID ID) -> Error {
    if (!Seen.insert(ID).second)
      return Error::success();

    Expected<cas::BlobRef> Blob = CAS.getBlob(ID);
    if (!Blob)
      return Blob.takeError();
    Worklist.push_back(*Blob);
    return Error::success();
  };

  // Find an overapproximation of 'include' directives.
  SmallVector<StringRef> IncludedFiles;
  while (!Worklist.empty()) {
    IncludedFiles.clear();
    if (Error E = scanTextForIncludes(CAS, ExecID, Worklist.pop_back_val(),
                                      IncludedFiles))
      return E;

    // Add included files to the worklist. Ignore files not found, since the
    // fuzzy search above could have found commented out includes.
    for (StringRef IncludedFile : IncludedFiles)
      if (Optional<cas::CASID> ID =
              lookupIncludeID(FS, IncludeDirs, IncludedFile))
        if (Error E = push(*ID))
          return E;
  }
  return Error::success();
}

static Expected<cas::BlobRef> openMainFile(cas::CachingOnDiskFileSystem &FS,
                                           StringRef MainFilename) {
  cas::CASDB &CAS = FS.getCAS();

  // FIXME: FileSystem should virtualize stdin.
  if (MainFilename == "-") {
    // Get stdin, then add it to the CAS.
    ErrorOr<std::unique_ptr<MemoryBuffer>> MaybeFile = MemoryBuffer::getSTDIN();
    if (!MaybeFile)
      return createMainFileError(MainFilename, MaybeFile.getError());
    return CAS.createBlob((**MaybeFile).getBuffer());
  }

  Optional<cas::CASID> MainFileID;
  if (std::error_code EC =
          FS.statusAndFileID(MainFilename, MainFileID).getError())
    return createMainFileError(MainFilename, EC);
  return CAS.getBlob(*MainFileID);
}

Error tablegen::createMainFileError(StringRef MainFilename,
                                    std::error_code EC) {
  assert(EC && "Expected error");
  return createStringError(EC, "Could not open input file '" + MainFilename +
                                   "': " + EC.message());
}

Expected<ScanIncludesResult> tablegen::scanIncludes(
    cas::CASDB &CAS, cas::CASID ExecID, StringRef MainFilename,
    ArrayRef<std::string> IncludeDirs, ArrayRef<MappedPrefix> PrefixMappings,
    Optional<RealPathPrefixMapper> *CapturedPM) {
  IntrusiveRefCntPtr<cas::CachingOnDiskFileSystem> FS;
  if (Error E = cas::createCachingOnDiskFileSystem(CAS).moveInto(FS))
    return std::move(E);

  FS->trackNewAccesses();
  Expected<cas::BlobRef> MainBlob = openMainFile(*FS, MainFilename);
  if (!MainBlob)
    return MainBlob.takeError();

  // Helper for adding to the worklist.
  if (Error E = accessAllIncludes(*FS, ExecID, IncludeDirs, *MainBlob))
    return std::move(E);

  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  Optional<RealPathPrefixMapper> LocalPM;
  Optional<RealPathPrefixMapper> &PM = CapturedPM ? *CapturedPM : LocalPM;
  if (!PrefixMappings.empty()) {
    PM.emplace(FS, Saver);
    if (Error E = PM->addRange(PrefixMappings))
      return std::move(E);
    PM->sort();
  }

  Expected<cas::TreeRef> Tree = FS->createTreeFromNewAccesses(
      [&](StringRef Path) { return PM ? PM->mapOrOriginal(Path) : Path; });
  if (!Tree)
    return Tree.takeError();

  return ScanIncludesResult{*Tree, *MainBlob};
}

Expected<ScanIncludesResult>
tablegen::scanIncludesAndRemap(cas::CASDB &CAS, cas::CASID ExecID,
                               std::string &MainFilename,
                               std::vector<std::string> &IncludeDirs,
                               ArrayRef<MappedPrefix> PrefixMappings) {
  Optional<RealPathPrefixMapper> PM;
  auto Result =
      scanIncludes(CAS, ExecID, MainFilename, IncludeDirs, PrefixMappings, &PM);
  if (!Result)
    return Result.takeError();

  if (!PM)
    return Result;

  // Remap the main filename. Since it was already loaded and should be cached
  // by the filesystem, an error here would be most surprising.
  if (Error E = PM->mapInPlace(MainFilename))
    return std::move(E);

  // Remap includes and strip invalid ones. If there have been no includes,
  // it's possible these will be new accesses, but they won't be needed anyway.
  // Clear and erase them.
  for (std::string &Dir : IncludeDirs)
    PM->mapInPlaceOrClear(Dir);
  llvm::erase_value(IncludeDirs, "");

  return Result;
}
