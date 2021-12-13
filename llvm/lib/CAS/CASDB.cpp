//===- CASDB.cpp ------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASDB.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/FileSystem.h"

using namespace llvm;
using namespace llvm::cas;

LLVM_DUMP_METHOD void CASDB::dump() const { print(dbgs()); }

Expected<std::string> CASDB::convertCASIDToString(CASID ID) {
  std::string Reference;
  {
    raw_string_ostream OS(Reference);
    if (Error E = printCASID(OS, ID))
      return std::move(E);
  }
  return std::move(Reference);
}

Error CASDB::getPrintedCASID(CASID ID, SmallVectorImpl<char> &Reference) {
  raw_svector_ostream OS(Reference);
  if (Error E = printCASID(OS, ID))
    return E;
  return Error::success();
}

/// Default implementation opens the file and calls \a createBlob().
Expected<BlobRef>
CASDB::createBlobFromOpenFileImpl(sys::fs::file_t FD,
                                  Optional<sys::fs::file_status> Status) {
  // Check whether we can trust the size from stat.
  int64_t FileSize = -1;
  if (Status->type() == sys::fs::file_type::regular_file ||
      Status->type() == sys::fs::file_type::block_file)
    FileSize = Status->getSize();

  // No need for a null terminator since the buffer will be dropped.
  ErrorOr<std::unique_ptr<MemoryBuffer>> ExpectedContent =
      MemoryBuffer::getOpenFile(FD, /*Filename=*/"", FileSize,
                                /*RequiresNullTerminator=*/false);
  if (!ExpectedContent)
    return errorCodeToError(ExpectedContent.getError());

  // FIXME: Change createBlob to return the right thing.
  return createBlob((*ExpectedContent)->getBuffer());
}
