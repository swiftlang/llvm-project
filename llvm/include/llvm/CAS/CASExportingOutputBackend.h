//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CAS_CASEXPORTINGOUTPUTBACKEND_H
#define LLVM_CAS_CASEXPORTINGOUTPUTBACKEND_H

#include "llvm/CAS/CASReference.h"
#include "llvm/Support/Compiler.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/VirtualOutputBackend.h"

namespace llvm::cas {
class ObjectProxy;
class ObjectStore;

/// An output backend that writes outputs both to a CAS and on disk, but writes
/// the content only once: when an output is kept, it's stored in the CAS,
/// exported from the CAS to the output path, which can clone the file backing
/// the object, and then passed to \a recordOutput(). Subclasses decide what to
/// do with the stored outputs.
class LLVM_ABI CASExportingOutputBackend : public vfs::OutputBackend {
  void anchor() override;

public:
  ObjectStore &getCAS() const { return CAS; }

  /// Write the data of \p Object on disk as the output for \p Path, e.g. to
  /// replay a cached output. \p Object is exported with
  /// \a vfs::OutputFile::fillAndKeep(), which can clone the file backing the
  /// object.
  static Error
  writeObject(const Twine &Path, ObjectProxy Object,
              std::optional<vfs::OutputConfig> Config = std::nullopt);

protected:
  explicit CASExportingOutputBackend(ObjectStore &CAS) : CAS(CAS) {}

  /// Creates an output that collects its content in memory, and that's stored,
  /// written on disk with \a writeObject(), and recorded with
  /// \a recordOutput() when it's kept.
  Expected<std::unique_ptr<vfs::OutputFileImpl>>
  createFileImpl(StringRef Path,
                 std::optional<vfs::OutputConfig> Config) override;

  /// Called with the stored \p Object once the output for \p Path is on disk.
  /// Can be called concurrently for different outputs.
  virtual Error recordOutput(StringRef Path, ObjectRef Object) = 0;

private:
  class CASOutputFile;

  ObjectStore &CAS;
};

} // namespace llvm::cas

#endif // LLVM_CAS_CASEXPORTINGOUTPUTBACKEND_H
