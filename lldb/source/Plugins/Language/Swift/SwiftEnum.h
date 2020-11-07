//===-- SwiftEnum.h ----------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
#ifndef liblldb_SwiftEnum_h_
#define liblldb_SwiftEnum_h_

#include "Plugins/TypeSystem/Swift/TypeSystemSwift.h"
#include "Plugins/TypeSystem/Swift/TypeSystemSwiftTypeRef.h"
#include "lldb/Core/SwiftForward.h"

#include "lldb/lldb-public.h"

namespace lldb_private {
class SwiftEnumDescriptor {
public:
  enum class Kind {
    Empty,      ///< No cases in this enum.
    CStyle,     ///< No cases have payloads.
    AllPayload, ///< All cases have payloads.
    Mixed,      ///< Some cases have payloads.
    Resilient   ///< A resilient enum.
  };

  struct ElementInfo {
    lldb_private::ConstString name;
    CompilerType payload_type;
    bool has_payload : 1;
    bool is_indirect : 1;
  };

  Kind GetKind() const { return m_kind; }

  ConstString GetTypeName() { return m_type_name; }

  virtual ElementInfo *
  GetElementFromData(const lldb_private::DataExtractor &data,
                     bool no_payload) = 0;

  virtual size_t GetNumElements() {
    return GetNumElementsWithPayload() + GetNumCStyleElements();
  }

  virtual size_t GetNumElementsWithPayload() = 0;

  virtual size_t GetNumCStyleElements() = 0;

  virtual ElementInfo *GetElementWithPayloadAtIndex(size_t idx) = 0;

  virtual ElementInfo *GetElementWithNoPayloadAtIndex(size_t idx) = 0;

  virtual ~SwiftEnumDescriptor() = default;

  static SwiftEnumDescriptor *
  GetEnumInfoFromEnumDecl(swift::ASTContext *ast, swift::CanType swift_can_type,
                          swift::EnumDecl *enum_decl);
protected:
  static SwiftEnumDescriptor *CreateDescriptor(swift::ASTContext *ast,
                                               swift::CanType swift_can_type,
                                               swift::EnumDecl *enum_decl);

  SwiftEnumDescriptor(swift::ASTContext *ast, swift::CanType swift_can_type,
                      swift::EnumDecl *enum_decl, SwiftEnumDescriptor::Kind k);

private:
  Kind m_kind;
  ConstString m_type_name;
};

typedef std::shared_ptr<SwiftEnumDescriptor> SwiftEnumDescriptorSP;

} // namespace lldb_private

#endif // liblldb_SwiftEnum_h_
