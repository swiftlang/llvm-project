//===-- SwiftEnum.cpp
//------------------------------------------------------===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2014 - 2020 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#include "swift/AST/Decl.h"
#include "swift/AST/Type.h"
#include "swift/AST/Types.h"
#include "swift/Basic/ClusteredBitVector.h"

#include "swift/../../lib/IRGen/FixedTypeInfo.h"
#include "swift/../../lib/IRGen/GenEnum.h"
#include "swift/../../lib/IRGen/GenHeap.h"
#include "swift/../../lib/IRGen/IRGenModule.h"
#include "swift/../../lib/IRGen/TypeInfo.h"

#include "Plugins/Language/Swift/SwiftEnum.h"
#include "Plugins/TypeSystem/Swift/SwiftASTContext.h"
#include "lldb/Utility/Log.h"
#include "lldb/Utility/Status.h"

using namespace lldb_private;

namespace std {
template <> struct less<swift::ClusteredBitVector> {
  bool operator()(const swift::ClusteredBitVector &lhs,
                  const swift::ClusteredBitVector &rhs) const {
    int iL = lhs.size() - 1;
    int iR = rhs.size() - 1;
    for (; iL >= 0 && iR >= 0; --iL, --iR) {
      bool bL = lhs[iL];
      bool bR = rhs[iR];
      if (bL && !bR)
        return false;
      if (bR && !bL)
        return true;
    }
    return false;
  }
};
} // namespace std

static std::string Dump(const swift::ClusteredBitVector &bit_vector) {
  std::string buffer;
  llvm::raw_string_ostream ostream(buffer);
  for (size_t i = 0; i < bit_vector.size(); i++) {
    if (bit_vector[i])
      ostream << '1';
    else
      ostream << '0';
    if ((i % 4) == 3)
      ostream << ' ';
  }
  ostream.flush();
  return buffer;
}

SwiftEnumDescriptor::SwiftEnumDescriptor(swift::ASTContext *ast,
                                         swift::CanType swift_can_type,
                                         swift::EnumDecl *enum_decl,
                                         SwiftEnumDescriptor::Kind k)
    : m_kind(k), m_type_name() {
  if (swift_can_type.getPointer()) {
    if (auto nominal = swift_can_type->getAnyNominal()) {
      swift::Identifier name(nominal->getName());
      if (name.get())
        m_type_name.SetCString(name.get());
    }
  }
}

class SwiftCStyleEnumDescriptor : public SwiftEnumDescriptor {
  llvm::SmallString<32> m_description = {"SwiftCStyleEnumDescriptor"};

public:
  SwiftCStyleEnumDescriptor(swift::ASTContext *ast,
                            swift::CanType swift_can_type,
                            swift::EnumDecl *enum_decl)
      : SwiftEnumDescriptor(ast, swift_can_type, enum_decl,
                            SwiftEnumDescriptor::Kind::CStyle),
        m_nopayload_elems_bitmask(), m_elements(), m_element_indexes() {
    Log *log = lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_TYPES);
    LLDB_LOG(log, "doing C-style enum layout for %s",
             GetTypeName().AsCString());

    SwiftASTContext *swift_ast_ctx = SwiftASTContext::GetSwiftASTContext(ast);
    swift::irgen::IRGenModule &irgen_module = swift_ast_ctx->GetIRGenModule();
    const swift::irgen::EnumImplStrategy &enum_impl_strategy =
        swift::irgen::getEnumImplStrategy(irgen_module, swift_can_type);
    llvm::ArrayRef<swift::irgen::EnumImplStrategy::Element>
        elements_with_no_payload =
            enum_impl_strategy.getElementsWithNoPayload();
    const bool has_payload = false;
    const bool is_indirect = false;
    uint64_t case_counter = 0;
    m_nopayload_elems_bitmask =
        enum_impl_strategy.getBitMaskForNoPayloadElements();

    if (enum_decl->isObjC())
      m_is_objc_enum = true;

    LLDB_LOG(log, "m_nopayload_elems_bitmask = %s",
             Dump(m_nopayload_elems_bitmask).c_str());

    for (auto enum_case : elements_with_no_payload) {
      ConstString case_name(enum_case.decl->getBaseIdentifier().str());
      swift::ClusteredBitVector case_value =
          enum_impl_strategy.getBitPatternForNoPayloadElement(enum_case.decl);

      LLDB_LOG(log, "case_name = %s, unmasked value = %s",
               case_name.AsCString(), Dump(case_value).c_str());

      case_value &= m_nopayload_elems_bitmask;

      LLDB_LOG(log, "case_name = %s, masked value = %s", case_name.AsCString(),
               Dump(case_value).c_str());

      std::unique_ptr<ElementInfo> elem_info(
          new ElementInfo{case_name, CompilerType(), has_payload, is_indirect});
      m_element_indexes.emplace(case_counter, elem_info.get());
      case_counter++;
      m_elements.emplace(case_value, std::move(elem_info));
    }
  }

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data, bool no_payload) {
    Log *log = lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_TYPES);
    LLDB_LOG(log,
             "C-style enum - inspecting data to find enum case for type %s",
             GetTypeName().AsCString());

    swift::ClusteredBitVector current_payload;
    lldb::offset_t offset = 0;
    for (size_t idx = 0; idx < data.GetByteSize(); idx++) {
      uint64_t byte = data.GetU8(&offset);
      current_payload.add(8, byte);
    }

    LLDB_LOG(log, "m_nopayload_elems_bitmask        = %s",
             Dump(m_nopayload_elems_bitmask).c_str());
    LLDB_LOG(log, "current_payload                  = %s",
             Dump(current_payload).c_str());

    if (current_payload.size() != m_nopayload_elems_bitmask.size()) {
      LLDB_LOG(log, "sizes don't match; getting out with an error");
      return nullptr;
    }

    // A C-Like Enum  is laid out as an integer tag with the minimal number of
    // bits to contain all of the cases. The cases are assigned tag values in
    // declaration order. e.g.
    // enum Patatino { // => LLVM i1
    // case X         // => i1 0
    // case Y         // => i1 1
    // }
    // From this we can find out the number of bits really used for the payload.
    if (!m_is_objc_enum) {
      current_payload &= m_nopayload_elems_bitmask;
      auto elem_mask =
          swift::ClusteredBitVector::getConstant(current_payload.size(), false);
      int64_t bit_count = m_elements.size() - 1;
      if (bit_count > 0 && no_payload) {
        uint64_t bit_set = 0;
        while (bit_count > 0) {
          elem_mask.setBit(bit_set);
          bit_set += 1;
          bit_count /= 2;
        }
        current_payload &= elem_mask;
      }
    }

    LLDB_LOG(log, "masked current_payload           = %s",
             Dump(current_payload).c_str());

    auto iter = m_elements.find(current_payload), end = m_elements.end();
    if (iter == end) {
      LLDB_LOG(log, "bitmask search failed");
      return nullptr;
    }
    LLDB_LOG(log, "bitmask search success - found case %s",
             iter->second.get()->name.AsCString());
    return iter->second.get();
  }

  virtual size_t GetNumElementsWithPayload() { return 0; }

  virtual size_t GetNumCStyleElements() { return m_elements.size(); }

  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) {
    return nullptr;
  }

  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) {
    if (idx >= m_element_indexes.size())
      return nullptr;
    return m_element_indexes[idx];
  }

  static bool classof(const SwiftEnumDescriptor *S) {
    return S->GetKind() == SwiftEnumDescriptor::Kind::CStyle;
  }

  virtual ~SwiftCStyleEnumDescriptor() = default;

private:
  swift::ClusteredBitVector m_nopayload_elems_bitmask;
  std::map<swift::ClusteredBitVector, std::unique_ptr<ElementInfo>> m_elements;
  std::map<uint64_t, ElementInfo *> m_element_indexes;
  bool m_is_objc_enum = false;
};

class SwiftAllPayloadEnumDescriptor : public SwiftEnumDescriptor {
  llvm::SmallString<32> m_description = {"SwiftAllPayloadEnumDescriptor"};

public:
  SwiftAllPayloadEnumDescriptor(swift::ASTContext *ast,
                                swift::CanType swift_can_type,
                                swift::EnumDecl *enum_decl)
      : SwiftEnumDescriptor(ast, swift_can_type, enum_decl,
                            SwiftEnumDescriptor::Kind::AllPayload) {
    Log *log = lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_TYPES);
    LLDB_LOG(log, "doing ADT-style enum layout for %s",
             GetTypeName().AsCString());

    SwiftASTContext *swift_ast_ctx = SwiftASTContext::GetSwiftASTContext(ast);
    swift::irgen::IRGenModule &irgen_module = swift_ast_ctx->GetIRGenModule();
    const swift::irgen::EnumImplStrategy &enum_impl_strategy =
        swift::irgen::getEnumImplStrategy(irgen_module, swift_can_type);
    llvm::ArrayRef<swift::irgen::EnumImplStrategy::Element>
        elements_with_payload = enum_impl_strategy.getElementsWithPayload();
    m_tag_bits = enum_impl_strategy.getTagBitsForPayloads();

    LLDB_LOG(log, "tag_bits = %s", Dump(m_tag_bits).c_str());

    auto module_ctx = enum_decl->getModuleContext();
    const bool has_payload = true;
    for (auto enum_case : elements_with_payload) {
      ConstString case_name(enum_case.decl->getBaseIdentifier().str());

      swift::EnumElementDecl *case_decl = enum_case.decl;
      assert(case_decl);
      auto arg_type = case_decl->getArgumentInterfaceType();
      CompilerType case_type;
      if (arg_type) {
        case_type = ToCompilerType(
            {swift_can_type->getTypeOfMember(module_ctx, case_decl, arg_type)
                 ->getCanonicalType()
                 .getPointer()});
      }

      const bool is_indirect =
          case_decl->isIndirect() || case_decl->getParentEnum()->isIndirect();

      LLDB_LOG(log, "case_name = %s, type = %s, is_indirect = %s",
               case_name.AsCString(), case_type.GetTypeName().AsCString(),
               is_indirect ? "yes" : "no");

      std::unique_ptr<ElementInfo> elem_info(
          new ElementInfo{case_name, case_type, has_payload, is_indirect});
      m_elements.push_back(std::move(elem_info));
    }
  }

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data, bool no_payload) {
    Log *log = lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_TYPES);
    LLDB_LOG(log,
             "ADT-style enum - inspecting data to find enum case for type %s",
             GetTypeName().AsCString());

    // No elements, just fail.
    if (m_elements.size() == 0) {
      LLDB_LOG(log, "enum with no cases. getting out");
      return nullptr;
    }
    // One element, so it's got to be it.
    if (m_elements.size() == 1) {
      LLDB_LOG(log, "enum with one case. getting out easy with %s",
               m_elements.front().get()->name.AsCString());

      return m_elements.front().get();
    }

    swift::ClusteredBitVector current_payload;
    lldb::offset_t offset = 0;
    for (size_t idx = 0; idx < data.GetByteSize(); idx++) {
      uint64_t byte = data.GetU8(&offset);
      current_payload.add(8, byte);
    }
    LLDB_LOG(log, "tag_bits        = %s", Dump(m_tag_bits).c_str());
    LLDB_LOG(log, "current_payload = %s", Dump(current_payload).c_str());

    if (current_payload.size() != m_tag_bits.size()) {
      LLDB_LOG(log, "sizes don't match; getting out with an error");
      return nullptr;
    }

    // FIXME: this assumes the tag bits should be gathered in
    // little-endian byte order.
    size_t discriminator = 0;
    size_t power_of_2 = 1;
    for (size_t i = 0; i < m_tag_bits.size(); ++i) {
      if (m_tag_bits[i]) {
        discriminator |= current_payload[i] ? power_of_2 : 0;
        power_of_2 <<= 1;
      }
    }

    // The discriminator is too large?
    if (discriminator >= m_elements.size()) {
      LLDB_LOG(log, "discriminator value of %" PRIu64 " too large, getting out",
               (uint64_t)discriminator);
      return nullptr;
    } else {
      auto ptr = m_elements[discriminator].get();
      if (!ptr)
        LLDB_LOG(log,
                 "discriminator value of %" PRIu64
                 " acceptable, but null case matched - that's bad",
                 (uint64_t)discriminator);
      else
        LLDB_LOG(log,
                 "discriminator value of %" PRIu64
                 " acceptable, case %s matched",
                 (uint64_t)discriminator, ptr->name.AsCString());
      return ptr;
    }
  }

  virtual size_t GetNumElementsWithPayload() { return m_elements.size(); }

  virtual size_t GetNumCStyleElements() { return 0; }

  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) {
    if (idx >= m_elements.size())
      return nullptr;
    return m_elements[idx].get();
  }

  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) {
    return nullptr;
  }

  static bool classof(const SwiftEnumDescriptor *S) {
    return S->GetKind() == SwiftEnumDescriptor::Kind::AllPayload;
  }

  virtual ~SwiftAllPayloadEnumDescriptor() = default;

private:
  swift::ClusteredBitVector m_tag_bits;
  std::vector<std::unique_ptr<ElementInfo>> m_elements;
};

class SwiftMixedEnumDescriptor : public SwiftEnumDescriptor {
public:
  SwiftMixedEnumDescriptor(swift::ASTContext *ast,
                           swift::CanType swift_can_type,
                           swift::EnumDecl *enum_decl)
      : SwiftEnumDescriptor(ast, swift_can_type, enum_decl,
                            SwiftEnumDescriptor::Kind::Mixed),
        m_non_payload_cases(ast, swift_can_type, enum_decl),
        m_payload_cases(ast, swift_can_type, enum_decl) {}

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data, bool no_payload) {
    ElementInfo *elem_info =
        m_non_payload_cases.GetElementFromData(data, false);
    return elem_info ? elem_info
                     : m_payload_cases.GetElementFromData(data, false);
  }

  static bool classof(const SwiftEnumDescriptor *S) {
    return S->GetKind() == SwiftEnumDescriptor::Kind::Mixed;
  }

  virtual size_t GetNumElementsWithPayload() {
    return m_payload_cases.GetNumElementsWithPayload();
  }

  virtual size_t GetNumCStyleElements() {
    return m_non_payload_cases.GetNumCStyleElements();
  }

  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) {
    return m_payload_cases.GetElementWithPayloadAtIndex(idx);
  }

  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) {
    return m_non_payload_cases.GetElementWithNoPayloadAtIndex(idx);
  }

  virtual ~SwiftMixedEnumDescriptor() = default;

private:
  SwiftCStyleEnumDescriptor m_non_payload_cases;
  SwiftAllPayloadEnumDescriptor m_payload_cases;
};

class SwiftResilientEnumDescriptor : public SwiftEnumDescriptor {
  llvm::SmallString<32> m_description = {"SwiftResilientEnumDescriptor"};

public:
  SwiftResilientEnumDescriptor(swift::ASTContext *ast,
                               swift::CanType swift_can_type,
                               swift::EnumDecl *enum_decl)
      : SwiftEnumDescriptor(ast, swift_can_type, enum_decl,
                            SwiftEnumDescriptor::Kind::Resilient) {
    Log *log = lldb_private::GetLogIfAllCategoriesSet(LIBLLDB_LOG_TYPES);
    LLDB_LOG(log, "doing resilient enum layout for %s",
             GetTypeName().AsCString());
  }

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data, bool no_payload) {
    // Not yet supported by LLDB.
    return nullptr;
  }
  virtual size_t GetNumElementsWithPayload() { return 0; }
  virtual size_t GetNumCStyleElements() { return 0; }
  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) {
    return nullptr;
  }
  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) {
    return nullptr;
  }
  static bool classof(const SwiftEnumDescriptor *S) {
    return S->GetKind() == SwiftEnumDescriptor::Kind::Resilient;
  }
  virtual ~SwiftResilientEnumDescriptor() = default;
};

class SwiftEmptyEnumDescriptor : public SwiftEnumDescriptor {
public:
  SwiftEmptyEnumDescriptor(swift::ASTContext *ast,
                           swift::CanType swift_can_type,
                           swift::EnumDecl *enum_decl)
      : SwiftEnumDescriptor(ast, swift_can_type, enum_decl,
                            SwiftEnumDescriptor::Kind::Empty) {}

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data, bool no_payload) {
    return nullptr;
  }

  virtual size_t GetNumElementsWithPayload() { return 0; }

  virtual size_t GetNumCStyleElements() { return 0; }

  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) {
    return nullptr;
  }

  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) {
    return nullptr;
  }

  static bool classof(const SwiftEnumDescriptor *S) {
    return S->GetKind() == SwiftEnumDescriptor::Kind::Empty;
  }

  virtual ~SwiftEmptyEnumDescriptor() = default;
};

SwiftEnumDescriptor *
SwiftEnumDescriptor::GetEnumInfoFromEnumDecl(swift::ASTContext *ast,
                                             swift::CanType swift_can_type,
                                             swift::EnumDecl *enum_decl) {
  return SwiftEnumDescriptor::CreateDescriptor(ast, swift_can_type, enum_decl);
}

SwiftEnumDescriptor *
SwiftEnumDescriptor::CreateDescriptor(swift::ASTContext *ast,
                                      swift::CanType swift_can_type,
                                      swift::EnumDecl *enum_decl) {
  assert(ast);
  assert(enum_decl);
  assert(swift_can_type.getPointer());
  SwiftASTContext *swift_ast_ctx = SwiftASTContext::GetSwiftASTContext(ast);
  assert(swift_ast_ctx);
  swift::irgen::IRGenModule &irgen_module = swift_ast_ctx->GetIRGenModule();
  const swift::irgen::EnumImplStrategy &enum_impl_strategy =
      swift::irgen::getEnumImplStrategy(irgen_module, swift_can_type);
  llvm::ArrayRef<swift::irgen::EnumImplStrategy::Element>
      elements_with_payload = enum_impl_strategy.getElementsWithPayload();
  llvm::ArrayRef<swift::irgen::EnumImplStrategy::Element>
      elements_with_no_payload = enum_impl_strategy.getElementsWithNoPayload();
  swift::SILType swift_sil_type = irgen_module.getLoweredType(swift_can_type);
  if (!irgen_module.getTypeInfo(swift_sil_type).isFixedSize())
    return new SwiftResilientEnumDescriptor(ast, swift_can_type, enum_decl);
  if (elements_with_no_payload.size() == 0) {
    // Nothing with no payload.. empty or all payloads?
    if (elements_with_payload.size() == 0)
      return new SwiftEmptyEnumDescriptor(ast, swift_can_type, enum_decl);
    return new SwiftAllPayloadEnumDescriptor(ast, swift_can_type, enum_decl);
  }

  // Something with no payload.. mixed or C-style?
  if (elements_with_payload.size() == 0)
    return new SwiftCStyleEnumDescriptor(ast, swift_can_type, enum_decl);
  return new SwiftMixedEnumDescriptor(ast, swift_can_type, enum_decl);
}
