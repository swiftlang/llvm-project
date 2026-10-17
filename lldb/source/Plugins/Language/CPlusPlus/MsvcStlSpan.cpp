//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "MsvcStl.h"

#include "lldb/DataFormatters/FormattersHelpers.h"
#include "lldb/Utility/ConstString.h"
#include "lldb/Utility/Scalar.h"
#include "lldb/ValueObject/ValueObject.h"
#include "llvm/Support/ErrorExtras.h"
#include <optional>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::formatters;

namespace lldb_private::formatters {

class MsvcStlSpanSyntheticFrontEnd : public SyntheticChildrenFrontEnd {
public:
  MsvcStlSpanSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp);

  ~MsvcStlSpanSyntheticFrontEnd() override = default;

  llvm::Expected<uint32_t> CalculateNumChildren() override {
    if (!m_num_elements)
      return llvm::createStringError(
          "could not determine the size of the span: neither a '_Mysize' "
          "member nor a '_Mysize' constant in its extent base class");
    return *m_num_elements;
  }

  lldb::ValueObjectSP GetChildAtIndex(uint32_t idx) override;

  lldb::ChildCacheState Update() override;

  llvm::Expected<size_t> GetIndexOfChildWithName(ConstString name) override;

private:
  ValueObject *m_start = nullptr; ///< First element of span. Held, not owned.
  CompilerType m_element_type{};  ///< Type of span elements.
  /// Number of elements in span, or std::nullopt if it could not be read.
  std::optional<size_t> m_num_elements;
  uint32_t m_element_size = 0; ///< Size in bytes of each span element.
};

lldb_private::formatters::MsvcStlSpanSyntheticFrontEnd::
    MsvcStlSpanSyntheticFrontEnd(lldb::ValueObjectSP valobj_sp)
    : SyntheticChildrenFrontEnd(*valobj_sp) {
  if (valobj_sp)
    Update();
}

lldb::ValueObjectSP
lldb_private::formatters::MsvcStlSpanSyntheticFrontEnd::GetChildAtIndex(
    uint32_t idx) {
  if (!m_start)
    return {};

  uint64_t offset = idx * m_element_size;
  offset = offset + m_start->GetValueAsUnsigned(0);
  StreamString name;
  name.Printf("[%" PRIu64 "]", (uint64_t)idx);
  return CreateChildValueObjectFromAddress(name.GetString(), offset,
                                           m_backend.GetExecutionContextRef(),
                                           m_element_type);
}

lldb::ChildCacheState
lldb_private::formatters::MsvcStlSpanSyntheticFrontEnd::Update() {
  m_start = nullptr;
  m_element_type = CompilerType();
  m_num_elements = std::nullopt;
  m_element_size = 0;

  ValueObjectSP data_sp = m_backend.GetChildMemberWithName("_Mydata");
  if (!data_sp)
    return lldb::ChildCacheState::eRefetch;

  m_element_type = data_sp->GetCompilerType().GetPointeeType();

  // Get element size.
  llvm::Expected<uint64_t> size_or_err = m_element_type.GetByteSize(nullptr);
  if (!size_or_err) {
    LLDB_LOG_ERRORV(GetLog(LLDBLog::DataFormatters), size_or_err.takeError(),
                    "{0}");
    return lldb::ChildCacheState::eRefetch;
  }

  m_element_size = *size_or_err;

  // Get data.
  if (m_element_size > 0)
    m_start = data_sp.get();

  // Get number of elements.
  if (auto size_sp = m_backend.GetChildMemberWithName("_Mysize")) {
    m_num_elements = size_sp->GetValueAsUnsigned(0);
  } else if (auto field =
                 m_backend.GetCompilerType()
                     .GetDirectBaseClassAtIndex(0, nullptr) // _Span_extent_type
                     .GetStaticFieldWithName("_Mysize")) {
    if (Scalar extent = field.GetConstantValue(); extent.IsValid())
      m_num_elements = extent.ULongLong(0);
  }

  return lldb::ChildCacheState::eRefetch;
}

llvm::Expected<size_t>
lldb_private::formatters::MsvcStlSpanSyntheticFrontEnd::GetIndexOfChildWithName(
    ConstString name) {
  if (!m_start)
    return llvm::createStringErrorV("type has no child named '{0}'", name);

  auto optional_idx = formatters::ExtractIndexFromString(name.GetCString());
  if (!optional_idx)
    return llvm::createStringErrorV("type has no child named '{0}'", name);
  return *optional_idx;
}

bool IsMsvcStlSpan(ValueObject &valobj) {
  if (auto valobj_sp = valobj.GetNonSyntheticValue())
    return valobj_sp->GetChildMemberWithName("_Mydata") != nullptr;
  return false;
}

lldb_private::SyntheticChildrenFrontEnd *
MsvcStlSpanSyntheticFrontEndCreator(CXXSyntheticChildren *,
                                    lldb::ValueObjectSP valobj_sp) {
  if (!valobj_sp)
    return nullptr;
  return new MsvcStlSpanSyntheticFrontEnd(valobj_sp);
}

} // namespace lldb_private::formatters
