//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASExportingOutputBackend.h"
#include "llvm/ADT/SmallString.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/IOSandbox.h"
#include "llvm/Support/raw_ostream.h"
#include <chrono>

using namespace llvm;
using namespace llvm::cas;

void CASExportingOutputBackend::anchor() {}

/// An output that collects its content in memory. When it's kept, the content
/// is stored in the CAS, written on disk, and recorded.
class CASExportingOutputBackend::CASOutputFile final
    : public vfs::OutputFileImpl {
public:
  Error keep() override {
    std::optional<ObjectProxy> Object;
    if (Error E = Backend.getCAS().createProxy({}, Bytes).moveInto(Object))
      return E;
    if (Error E = writeObject(Path, *Object, Config))
      return E;
    return Backend.recordOutput(Path, Object->getRef());
  }
  Error discard() override { return Error::success(); }
  raw_pwrite_stream &getOS() override { return OS; }

  CASOutputFile(CASExportingOutputBackend &Backend, StringRef Path,
                std::optional<vfs::OutputConfig> Config)
      : Backend(Backend), Path(Path.str()), Config(Config), OS(Bytes) {}

private:
  CASExportingOutputBackend &Backend;
  std::string Path;
  std::optional<vfs::OutputConfig> Config;
  SmallString<16> Bytes;
  raw_svector_ostream OS;
};

Expected<std::unique_ptr<vfs::OutputFileImpl>>
CASExportingOutputBackend::createFileImpl(
    StringRef Path, std::optional<vfs::OutputConfig> Config) {
  return std::make_unique<CASOutputFile>(*this, Path, Config);
}

Error CASExportingOutputBackend::writeObject(
    const Twine &Path, ObjectProxy Object,
    std::optional<vfs::OutputConfig> Config) {
  SmallString<128> PathStorage;
  StringRef PathRef = Path.toStringRef(PathStorage);

  std::unique_ptr<vfs::OutputFileImpl> Impl;
  if (Error E = createFileOnDisk(PathRef, Config).moveInto(Impl))
    return E;

  vfs::OutputFile File(PathRef, std::move(Impl));
  return File.fillAndKeep([&](StringRef FillPath) -> Error {
    // Exporting can clone the file backing the object, which keeps the
    // timestamps of that file. The output should look newly written.
    if (Error E = Object.exportDataToFile(FillPath))
      return E;
    auto BypassSandbox = sys::sandbox::scopedDisable();
    if (std::error_code EC = sys::fs::setLastAccessAndModificationTime(
            FillPath, std::chrono::system_clock::now()))
      return createFileError(FillPath, EC);
    return Error::success();
  });
}
