//===-- SwiftStdlibFormatters.cpp -------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "SwiftStdlibFormatters.h"

#include "lldb/Utility/ConstString.h"
#include "lldb/ValueObject/ValueObject.h"
#include "lldb/lldb-enumerations.h"
#include "llvm/Support/Error.h"

using namespace lldb;
using namespace lldb_private;

namespace {

/// Synthetic children provider for Swift.UniqueBox<T>.
///
/// Exposes one child:
///   [0] value - generic value stored in the box
class UniqueBoxSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  UniqueBoxSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
      : SyntheticChildrenFrontEnd(*valobj_sp) {}

  llvm::Expected<uint32_t> CalculateNumChildren() override { return 1; }

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override {
    return idx == 0 ? m_value_sp : nullptr;
  }

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override {
    if (name == "value")
      return 0;
    return llvm::createStringErrorV("Type has no child named '{0}'", name);
  }

  lldb::ChildCacheState Update() override {
    auto value_sp = m_backend.GetValueForExpressionPath(".pointer.pointee");
    if (!value_sp) {
      m_value_sp = nullptr;
      return ChildCacheState::eRefetch;
    }

    m_value_sp = value_sp->Clone(ConstString("value"));
    return ChildCacheState::eReuse;
  }

private:
  lldb::ValueObjectSP m_value_sp;
};

} // namespace

SyntheticChildrenFrontEnd *
lldb_private::formatters::swift::UniqueBoxSyntheticFrontEndCreator(
    CXXSyntheticChildren *, lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return nullptr;
  return new UniqueBoxSyntheticFrontEnd(valobj_sp);
}
