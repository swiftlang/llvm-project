//===- CASOutputBackend.cpp -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASOutputBackend.h"
#include "llvm/ADT/StringMap.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/HierarchicalTreeBuilder.h"
#include "llvm/CAS/Utils.h"
#include "llvm/Support/AlignOf.h"
#include "llvm/Support/Allocator.h"

using namespace llvm;
using namespace llvm::cas;

namespace {
class CASOutputFile : public vfs::OutputFile {
public:
  Error storeContentBuffer(ContentBuffer &Content) override {
    return OnClose(getPath(), Content.getBytes());
  }

  using OnCloseType = llvm::unique_function<Error(StringRef, StringRef)>;

  CASOutputFile(StringRef Path, OnCloseType OnClose)
      : OutputFile(Path), OnClose(std::move(OnClose)) {}

  static bool isNull(const OutputFile &File) {
    return OutputFile::isNull(File);
  }

private:
  OnCloseType OnClose;
};
} // namespace

CASOutputBackend::CASOutputBackend(
    std::shared_ptr<CASDB> CAS,
    IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDOutputBackend)
    : CASOutputBackend(*CAS, std::move(CASIDOutputBackend)) {
  this->OwnedCAS = std::move(CAS);
}

CASOutputBackend::CASOutputBackend(
    CASDB &CAS, IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDOutputBackend)
    : StableUniqueEntityAdaptorType(
          sys::path::Style::native /*FIXME: should be posix?*/),
      CAS(CAS), CASIDOutputBackend(std::move(CASIDOutputBackend)) {}

CASOutputBackend::~CASOutputBackend() = default;

struct CASOutputBackend::PrivateImpl {
  HierarchicalTreeBuilder Builder;
};

static Error writeOutputAsCASID(CASDB &CAS, CASID &ID, StringRef ResolvedPath,
                                llvm::vfs::OutputBackend &CASIDOutputBackend,
                                vfs::OutputConfig Config) {
  auto ExpOutFile = CASIDOutputBackend.createFile(ResolvedPath, Config);
  if (!ExpOutFile)
    return ExpOutFile.takeError();
  auto OutFile = std::move(*ExpOutFile);
  if (CASOutputFile::isNull(*OutFile))
    return Error::success();

  writeCASIDBuffer(CAS, ID, *OutFile->getOS());

  SmallString<50> Contents;
  {
    raw_svector_ostream OS(Contents);
    writeCASIDBuffer(CAS, ID, OS);
  }
  Expected<BlobRef> Blob = CAS.createBlob(Contents);
  if (!Blob)
    return Blob.takeError();
  ID = *Blob;

  return OutFile->close();
}

Expected<std::unique_ptr<vfs::OutputFile>>
CASOutputBackend::createFileImpl(StringRef ResolvedPath,
                                 vfs::OutputConfig Config) {
  if (!Impl)
    Impl = std::make_unique<PrivateImpl>();

  return std::make_unique<CASOutputFile>(
      ResolvedPath, [&](StringRef Path, StringRef Bytes) -> Error {
        Expected<BlobRef> Blob = CAS.createBlob(Bytes);
        if (!Blob)
          return Blob.takeError();
        CASID ID = *Blob;
        if (CASIDOutputBackend) {
          if (Error E = writeOutputAsCASID(CAS, ID, Path, *CASIDOutputBackend,
                                           Config))
            return E;
        }
        Impl->Builder.push(ID, TreeEntry::Regular, Path);
        return Error::success();
      });
}

Expected<TreeRef> CASOutputBackend::createTree() {
  if (!Impl)
    return CAS.createTree();

  Expected<TreeRef> ExpectedTree = Impl->Builder.create(CAS);
  Impl->Builder.clear();
  return ExpectedTree;
}
