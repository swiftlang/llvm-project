//===-- ScriptInterpreterSwift.cpp ----------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "ScriptInterpreterSwift.h"
#include "SwiftInterpreter.h"

#include "lldb/Core/Debugger.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Interpreter/CommandReturnObject.h"

#include "llvm/Support/FormatAdapters.h"

#include <memory>
#include <vector>

using namespace lldb;
using namespace lldb_private;

LLDB_PLUGIN_DEFINE(ScriptInterpreterSwift)

ScriptInterpreterSwift::ScriptInterpreterSwift(Debugger &Debugger)
    : ScriptInterpreter(Debugger, eScriptLanguageSwift) {}

ScriptInterpreterSwift::~ScriptInterpreterSwift() {}

bool ScriptInterpreterSwift::ExecuteOneLine(
    llvm::StringRef Command, CommandReturnObject *Result,
    const ExecuteScriptOptions &Options) {
  if (Command.empty()) {
    if (Result)
      Result->AppendError("empty command passed to swift\n");
    return false;
  }

  llvm::Expected<std::unique_ptr<ScriptInterpreterIORedirect>>
      IORedirectOrError = ScriptInterpreterIORedirect::Create(
          Options.GetEnableIO(), m_debugger, Result);
  if (!IORedirectOrError) {
    if (Result)
      Result->AppendErrorWithFormatv(
          "failed to redirect I/O: {0}\n",
          llvm::fmt_consume(IORedirectOrError.takeError()));
    else
      llvm::consumeError(IORedirectOrError.takeError());
    return false;
  }

  ScriptInterpreterIORedirect &IORedirect = **IORedirectOrError;

  if (auto Error = m_swift->executeOneLine(Command, m_debugger)) {
    Result->AppendErrorWithFormatv(
        "Swift failed attempting to evaluate '{0}': {1}\n", Command,
        llvm::toString(std::move(Error)));
    return false;
  }

  IORedirect.Flush();
  return true;
}

void ScriptInterpreterSwift::ExecuteInterpreterLoop() {
  // At the moment, the only time the debugger does not have an input file
  // handle is when this is called directly from lua, in which case it is
  // both dangerous and unnecessary (not to mention confusing) to try to embed
  // a running interpreter loop inside the already running lua interpreter
  // loop, so we won't do it.
  if (!m_debugger.GetInputFile().IsValid())
    return;

  // FIXME: unimplemented
}

bool ScriptInterpreterSwift::LoadScriptingModule(
    const char *Filename, const LoadScriptOptions &options,
    lldb_private::Status &Error, StructuredData::ObjectSP *Module_sp,
    FileSpec ExtraSearchDir) {

  FileSystem::Instance().Collect(Filename);
  if (llvm::Error E = m_swift->loadScriptFile(Filename)) {
    Error.SetErrorStringWithFormatv("swift failed to import '{0}': {1}\n",
                                    Filename, llvm::toString(std::move(E)));
    return false;
  }
  return true;
}

void ScriptInterpreterSwift::Initialize() {
  static llvm::once_flag g_onceFlag;

  llvm::call_once(g_onceFlag, []() {
    PluginManager::RegisterPlugin(GetPluginNameStatic(),
                                  GetPluginDescriptionStatic(),
                                  lldb::eScriptLanguageSwift, CreateInstance);
  });
}

void ScriptInterpreterSwift::Terminate() {}

llvm::Error ScriptInterpreterSwift::EnterSession(user_id_t DebuggerId) {
  if (m_sessionIsActive)
    return llvm::Error::success();

  m_sessionIsActive = true;

  return llvm::Error::success();
}

llvm::Error ScriptInterpreterSwift::LeaveSession() {
  if (!m_sessionIsActive)
    return llvm::Error::success();

  m_sessionIsActive = false;

  return llvm::Error::success();
}

lldb::ScriptInterpreterSP
ScriptInterpreterSwift::CreateInstance(Debugger &Debugger) {
  return std::make_shared<ScriptInterpreterSwift>(Debugger);
}

lldb_private::ConstString ScriptInterpreterSwift::GetPluginNameStatic() {
  static ConstString g_name("script-swift");
  return g_name;
}

const char *ScriptInterpreterSwift::GetPluginDescriptionStatic() {
  return "Swift script interpreter";
}

lldb_private::ConstString ScriptInterpreterSwift::GetPluginName() {
  return GetPluginNameStatic();
}

uint32_t ScriptInterpreterSwift::GetPluginVersion() { return 1; }
