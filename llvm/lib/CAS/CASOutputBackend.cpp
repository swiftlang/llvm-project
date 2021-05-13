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

private:
  OnCloseType OnClose;
};
} // namespace

CASOutputBackend::CASOutputBackend(std::shared_ptr<CASDB> CAS)
    : CASOutputBackend(*CAS) {
  this->OwnedCAS = std::move(CAS);
}

CASOutputBackend::CASOutputBackend(CASDB &CAS)
    : StableUniqueEntityAdaptorType(
          sys::path::Style::native /*FIXME: should be posix?*/),
      CAS(CAS) {}

CASOutputBackend::~CASOutputBackend() = default;

struct CASOutputBackend::PrivateImpl {
  HierarchicalTreeBuilder Builder;
};

Expected<std::unique_ptr<vfs::OutputFile>>
CASOutputBackend::createFileImpl(StringRef ResolvedPath, vfs::OutputConfig) {
  if (!Impl)
    Impl = std::make_unique<PrivateImpl>();

  return std::make_unique<CASOutputFile>(
      ResolvedPath, [&](StringRef Path, StringRef Bytes) -> Error {
        Expected<BlobRef> Blob = CAS.createBlob(Bytes);
        if (!Blob)
          return Blob.takeError();
        Impl->Builder.push(*Blob, TreeEntry::Regular, Path);
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
