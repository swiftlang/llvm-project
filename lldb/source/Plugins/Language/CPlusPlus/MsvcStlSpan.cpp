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
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/ErrorExtras.h"
#include <limits>
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
          "could not determine the size of the span: it has no '_Mysize' "
          "member, and its extent is in neither a template argument, a "
          "'_Mysize' constant, nor the type name");
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

/// Extracts the trailing integral template argument from a type name, e.g. 5
/// from "std::span<int, 5>".
static std::optional<uint64_t> ExtentFromTypeName(llvm::StringRef name) {
  if (!name.consume_back(">"))
    return std::nullopt;

  size_t depth = 0;
  size_t separator = llvm::StringRef::npos;
  for (size_t i = name.size(); i-- > 0;) {
    if (name[i] == '>')
      ++depth;
    else if (name[i] == '<') {
      if (depth == 0)
        break;
      --depth;
    } else if (name[i] == ',' && depth == 0) {
      separator = i;
      break;
    }
  }
  if (separator == llvm::StringRef::npos)
    return std::nullopt;

  uint64_t extent;
  if (name.substr(separator + 1).trim().getAsInteger(10, extent))
    return std::nullopt;
  return extent;
}

/// Returns the element count of a span whose extent is part of its type.
///
/// A static extent is not stored: MSVC's `_Span_extent_type` holds only
/// `_Mydata` and derives `_Mysize` from the `_Extent` template argument, so the
/// count has to be recovered from the type itself. Which spelling survives into
/// the debug info varies, hence the three attempts: NativePDB rebuilds no
/// template arguments, and an STL that names the extent directly rather than
/// re-declaring it as a static member leaves nothing but the type name.
static std::optional<size_t> GetStaticExtent(CompilerType span_type) {
  // A dynamic extent means the size is a member instead, so reject the
  // sentinel rather than reporting SIZE_MAX children.
  auto if_static = [](uint64_t extent) -> std::optional<size_t> {
    if (extent == std::numeric_limits<uint64_t>::max())
      return std::nullopt;
    return extent;
  };

  if (auto arg = span_type.GetIntegralTemplateArgument(1))
    return if_static(arg->value.GetAPSInt().getLimitedValue());

  if (auto field = span_type.GetDirectBaseClassAtIndex(0, nullptr)
                       .GetStaticFieldWithName("_Mysize"))
    if (Scalar extent = field.GetConstantValue(); extent.IsValid())
      return if_static(extent.ULongLong(0));

  if (auto extent = ExtentFromTypeName(span_type.GetTypeName().GetStringRef()))
    return if_static(*extent);

  return std::nullopt;
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
  if (auto size_sp = m_backend.GetChildMemberWithName("_Mysize"))
    m_num_elements = size_sp->GetValueAsUnsigned(0);
  else
    m_num_elements = GetStaticExtent(m_backend.GetCompilerType());

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
