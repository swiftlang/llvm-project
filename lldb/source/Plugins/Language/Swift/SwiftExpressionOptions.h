//===-- SwiftExpressionOptions.h --------------------------------*- C++ -*-===//
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

#ifndef liblldb_SwiftExpressionOptions_h_
#define liblldb_SwiftExpressionOptions_h_

#include "lldb/Interpreter/OptionGroupLanguageExpression.h"
#include "lldb/lldb-enumerations.h"

namespace lldb_private {

/// The options that Swift adds to the commands that evaluate expressions.
class SwiftExpressionOptions : public OptionGroupLanguageExpression {
public:
  llvm::ArrayRef<OptionDefinition> GetDefinitions() override;

  Status SetOptionValue(uint32_t option_idx, llvm::StringRef option_arg,
                        ExecutionContext *execution_context) override;

  void OptionParsingStarting(ExecutionContext *execution_context) override;

  void Apply(EvaluateExpressionOptions &options) const override;

  /// Returns whether an expression that is evaluated with \p options binds the
  /// generic types in its context to their dynamic types.
  static lldb::BindGenericTypes
  GetBindGenericTypes(const EvaluateExpressionOptions &options);

  /// Sets whether an expression that is evaluated with \p options binds the
  /// generic types in its context to their dynamic types.
  static void SetBindGenericTypes(EvaluateExpressionOptions &options,
                                  bool bind_generic_types);

private:
  lldb::BindGenericTypes m_bind_generic_types = lldb::eBindAuto;
};

} // namespace lldb_private

#endif // liblldb_SwiftExpressionOptions_h_
