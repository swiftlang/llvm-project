//===-- SwiftInterpreter.h --------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_SwiftInterpreter_h_
#define liblldb_SwiftInterpreter_h_

#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/Support/Error.h"

#include "swift/Frontend/Frontend.h"
#include "swift/Frontend/PrintingDiagnosticConsumer.h"

using llvm::Error;
using llvm::Expected;

namespace lldb_private {
class Debugger;
}

namespace lldb_private {
class SwiftInterpreter {
public:
  SwiftInterpreter();

  Error executeOneLine(llvm::StringRef command, Debugger &debugger);
  Error loadScriptFile(llvm::StringRef filename);

private:
  Error initJIT();
  Error initCompiler();
  Error initialize();

  Expected<llvm::orc::JITDylib &> createJITDylib();
  Error executeMemoryBuffer(std::unique_ptr<llvm::MemoryBuffer> code);

  bool m_inited = false;
  int m_current_id = 0;

  std::unique_ptr<llvm::orc::LLJIT> m_jit = {};
  std::unique_ptr<swift::CompilerInvocation> m_compiler_invocation = {};
  std::unique_ptr<swift::CompilerInstance> m_compiler_instance = {};
  std::vector<swift::AttributedImport<swift::ImportedModule>> m_modules = {};

  std::unique_ptr<swift::PrintingDiagnosticConsumer> m_diagnostic_consumer = {};
  std::vector<llvm::orc::JITDylib *> m_dylibs;
};
} // namespace lldb_private

#endif
