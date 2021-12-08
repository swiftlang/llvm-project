//===- llvm/CAS/CASOutputBackend.h ----------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASOUTPUTBACKEND_H
#define LLVM_CAS_CASOUTPUTBACKEND_H

#include <llvm/Support/Error.h>
#include <llvm/Support/OutputBackend.h>

namespace llvm {
namespace cas {
class CASDB;
class CASID;
class TreeRef;

/// Handle the cas
class CASOutputBackend final : public vfs::StableUniqueEntityAdaptor<> {
public:
  /// Create a top-level tree for all created files. This will contain all files
  Expected<TreeRef> createTree();

  Expected<std::unique_ptr<vfs::OutputFile>>
  createFileImpl(StringRef ResolvedPath, vfs::OutputConfig Config) override;

  /// \param CASIDOutputBackend if set it will be used to write out the file
  /// contents as embedded CASID. Can be \p nullptr.
  CASOutputBackend(
      std::shared_ptr<CASDB> CAS,
      IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDOutputBackend);
  /// \param CASIDOutputBackend if set it will be used to write out the file
  /// contents as embedded CASID. Can be \p nullptr.
  CASOutputBackend(
      CASDB &CAS,
      IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDOutputBackend);

private:
  ~CASOutputBackend();
  struct PrivateImpl;
  std::unique_ptr<PrivateImpl> Impl;

  CASDB &CAS;
  std::shared_ptr<CASDB> OwnedCAS;
  IntrusiveRefCntPtr<llvm::vfs::OutputBackend> CASIDOutputBackend;
};

} // namespace cas
} // namespace llvm

#endif // LLVM_CAS_CASOUTPUTBACKEND_H
