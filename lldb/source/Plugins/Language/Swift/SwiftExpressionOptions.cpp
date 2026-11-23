//===-- SwiftExpressionOptions.cpp ----------------------------------------===//
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

#include "SwiftExpressionOptions.h"
#include "lldb/Host/OptionParser.h"
#include "lldb/Interpreter/CommandOptionArgumentTable.h"
#include "lldb/Interpreter/OptionArgParser.h"
#include "lldb/Target/Target.h"

using namespace lldb;
using namespace lldb_private;

#define LLDB_OPTIONS_expression_swift
#include "LanguageSwiftOptions.inc"

/// The language option that stores the value of --bind-generic-types.
static constexpr llvm::StringLiteral g_bind_generic_types_option =
    "swift-bind-generic-types";

llvm::ArrayRef<OptionDefinition> SwiftExpressionOptions::GetDefinitions() {
  return llvm::ArrayRef(g_expression_swift_options);
}

Status
SwiftExpressionOptions::SetOptionValue(uint32_t option_idx,
                                       llvm::StringRef option_arg,
                                       ExecutionContext *execution_context) {
  Status error;
  switch (GetDefinitions()[option_idx].short_option) {
  case '\x31': {
    int64_t result = OptionArgParser::ToOptionEnum(
        option_arg, BindGenTypeParamValue(), 0, error);
    if (error.Success())
      m_bind_generic_types = static_cast<BindGenericTypes>(result);
    break;
  }
  default:
    llvm_unreachable("Unimplemented option");
  }
  return error;
}

void SwiftExpressionOptions::OptionParsingStarting(
    ExecutionContext *execution_context) {
  m_bind_generic_types = eBindAuto;
}

void SwiftExpressionOptions::Apply(EvaluateExpressionOptions &options) const {
  if (m_bind_generic_types != eBindAuto)
    SetBindGenericTypes(options, m_bind_generic_types == eBind);
}

BindGenericTypes SwiftExpressionOptions::GetBindGenericTypes(
    const EvaluateExpressionOptions &options) {
  llvm::Expected<bool> bind_generic_types =
      options.GetBooleanLanguageOption(g_bind_generic_types_option);
  if (!bind_generic_types) {
    llvm::consumeError(bind_generic_types.takeError());
    return eBindAuto;
  }
  return *bind_generic_types ? eBind : eDontBind;
}

void SwiftExpressionOptions::SetBindGenericTypes(
    EvaluateExpressionOptions &options, bool bind_generic_types) {
  llvm::cantFail(options.SetBooleanLanguageOption(g_bind_generic_types_option,
                                                  bind_generic_types));
}
