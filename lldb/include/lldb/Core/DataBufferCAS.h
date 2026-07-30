//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
///
/// \file
/// A DataBuffer whose bytes are owned by a CAS ObjectStore.
///
//===----------------------------------------------------------------------===//

#ifndef LLDB_CORE_DATABUFFERCAS_H
#define LLDB_CORE_DATABUFFERCAS_H

#include "lldb/Utility/DataBufferLLVM.h"

#include <memory>
#include <utility>

namespace llvm {
class MemoryBuffer;
namespace cas {
class ObjectStore;
} // namespace cas
} // namespace llvm

namespace lldb_private {

/// A DataBuffer whose bytes are owned by a CAS \c ObjectStore.
///
/// ObjectStore::getMemoryBuffer() returns a non-owning buffer aliasing
/// storage the ObjectStore owns, so releasing the last reference to the
/// ObjectStore invalidates it; this buffer holds a reference to prevent
/// that, regardless of teardown order.
class DataBufferCAS : public DataBufferLLVM {
public:
  /// \param buffer A non-owning buffer aliasing storage owned by \p cas. Must
  ///        be a valid pointer.
  /// \param cas The ObjectStore that owns \p buffer's bytes.
  DataBufferCAS(std::unique_ptr<llvm::MemoryBuffer> buffer,
                std::shared_ptr<llvm::cas::ObjectStore> cas)
      : DataBufferLLVM(std::move(buffer)), m_cas(std::move(cas)) {}

private:
  /// Destroyed before the base class releases \c Buffer: safe, since the
  /// aliasing MemoryBuffer's destructor never dereferences its bytes.
  std::shared_ptr<llvm::cas::ObjectStore> m_cas;
};

} // namespace lldb_private

#endif // LLDB_CORE_DATABUFFERCAS_H
