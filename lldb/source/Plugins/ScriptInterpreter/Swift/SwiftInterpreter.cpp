//===-- SwiftInterpreter.cpp ----------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "SwiftInterpreter.h"

#include "Plugins/ExpressionParser/Swift/SwiftHost.h"
#include "Plugins/Platform/MacOSX/PlatformDarwin.h"
#include "Plugins/TypeSystem/Swift/SwiftASTContext.h"
#include "lldb/API/SBDebugger.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Expression/IRExecutionUnit.h"
#include "lldb/Host/HostInfo.h"

#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/Support/FormatVariadic.h"
#include "llvm/Support/ScopedPrinter.h"

#include "swift/AST/IRGenRequests.h"
#include "swift/Frontend/ModuleInterfaceLoader.h"
#include "swift/SIL/TypeLowering.h"
#include "swift/SILOptimizer/PassManager/Passes.h"
#include "swift/Serialization/SerializedModuleLoader.h"
#include "swift/Subsystems.h"

#include <dlfcn.h>

using namespace lldb;
using namespace lldb_private;

SwiftInterpreter::SwiftInterpreter() {
  // Make sure the Swift runtime is loaded.
  static auto swiftCore_handle =
      dlopen("/usr/lib/swift/libswiftCore.dylib", RTLD_LAZY);
  assert(swiftCore_handle);
}

Error SwiftInterpreter::initJIT() {
  llvm::orc::LLJITBuilder builder;

  auto maybe_jit = builder.create();
  if (!maybe_jit)
    return maybe_jit.takeError();
  m_jit = std::move(*maybe_jit);

  return Error::success();
}

Expected<llvm::orc::JITDylib &> SwiftInterpreter::createJITDylib() {
  int dylib_id = m_current_id++;

  auto jit_dylib_name =
      llvm::to_string(llvm::formatv("__lldb_swift_script_dylib_{0}", dylib_id));
  auto maybe_dylib = m_jit->createJITDylib(jit_dylib_name);
  if (!maybe_dylib)
    return maybe_dylib.takeError();

  // Make all the previous JIT dylibs available.
  // FIXME: MatchAllSymbols shouldn't be necessary but some examples break
  // without this.
  for (auto *previous_dylib : m_dylibs)
    maybe_dylib->addToLinkOrder(
        *previous_dylib, llvm::orc::JITDylibLookupFlags::MatchAllSymbols);

  // Add a generator so that the script can use symbols from the main
  // executable image. This could be restricted to something which just provides
  // the SB symbols and the swift runtime symbols.
  auto maybe_searcher =
      llvm::orc::DynamicLibrarySearchGenerator::GetForCurrentProcess(
          m_jit->getDataLayout().getGlobalPrefix());
  if (!maybe_searcher)
    return maybe_searcher.takeError();
  maybe_dylib->addGenerator(std::move(*maybe_searcher));

  m_dylibs.emplace_back(&*maybe_dylib);
  return *m_dylibs.back();
}

Error SwiftInterpreter::initCompiler() {
  // Create CompilerInstance with C++ interop.
  m_compiler_invocation = std::make_unique<swift::CompilerInvocation>();
  auto triple =
      SwiftASTContext::GetSwiftFriendlyTriple(HostInfoBase::GetTargetTriple());
  m_compiler_invocation->setTargetTriple(triple);
  m_compiler_invocation->getFrontendOptions().ModuleName = "lldb";

  swift::IRGenOptions &irgen_opts = m_compiler_invocation->getIRGenOptions();
  irgen_opts.OutputKind = swift::IRGenOutputKind::Module;
  irgen_opts.UseJIT = true;
  swift::LangOptions &lang_opts = m_compiler_invocation->getLangOptions();
  lang_opts.EnableObjCInterop = true;
  lang_opts.EnableCXXInterop = true;

  std::string sdk_path =
      HostInfo::GetXcodeSDKPath(XcodeSDK::GetAnyMacOS()).str();
  std::string stdlib_os_dir = SwiftASTContext::GetSwiftStdlibOSDir(
      triple, HostInfo::GetArchitecture().GetTriple());
  auto resource_dir = SwiftASTContext::GetResourceDir(
      sdk_path, stdlib_os_dir, GetSwiftResourceDir().GetPath(),
      HostInfo::GetXcodeContentsDirectory().GetPath(),
      PlatformDarwin::GetCurrentToolchainDirectory().GetPath(),
      PlatformDarwin::GetCurrentCommandLineToolsDirectory().GetPath());

  // Setup paths for the ClangImporter to find the SDK. The SB headers
  // depend on some SDK headers.
  swift::ClangImporterOptions &clang_importer_options =
      m_compiler_invocation->getClangImporterOptions();
  clang_importer_options.ExtraArgs.push_back("-isysroot");
  clang_importer_options.ExtraArgs.push_back(sdk_path);

  llvm::errs() << "resource_dir is " << resource_dir << "\n";
  // Allow the compiler to find the Swift standard library.
  m_compiler_invocation->setRuntimeResourcePath(resource_dir);

  // Finally create the CompilerInstance.
  m_compiler_instance = std::make_unique<swift::CompilerInstance>();

  // Display diagnostics to stderr.
  m_diagnostic_consumer = std::make_unique<swift::PrintingDiagnosticConsumer>();
  m_compiler_instance->addDiagnosticConsumer(m_diagnostic_consumer.get());

  if (m_compiler_instance->setup(*m_compiler_invocation)) {
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to setup CompilerInstance");
  }

  // Allow the compiler to find the LLDB framework.
  // FIXME: It's unclear whether this is correct after LLDB is installed.
  auto &ast_context = m_compiler_instance->getASTContext();
  ast_context.addSearchPath(resource_dir + "/../../..", true /* isFramework */,
                            false /* isSystem */);
  return Error::success();
}

Error SwiftInterpreter::initialize() {
  if (m_inited)
    return Error::success();

  if (auto error = initJIT())
    return error;
  if (auto error = initCompiler())
    return error;
  m_inited = true;
  return Error::success();
}

Error SwiftInterpreter::loadScriptFile(StringRef filename) {
  FileSpec file(filename);
  if (!FileSystem::Instance().Exists(file))
    return llvm::make_error<llvm::StringError>("invalid path",
                                               llvm::inconvertibleErrorCode());

  ConstString module_extension = file.GetFileNameExtension();
  if (module_extension != ".swift")
    return llvm::make_error<llvm::StringError>("invalid extension",
                                               llvm::inconvertibleErrorCode());

  auto maybe_mem_buffer = llvm::MemoryBuffer::getFile(filename);
  if (!maybe_mem_buffer)
    return llvm::make_error<llvm::StringError>("Error reading script file",
                                               maybe_mem_buffer.getError());

  return executeMemoryBuffer(std::move(*maybe_mem_buffer));
}

Error SwiftInterpreter::executeOneLine(StringRef line, Debugger &debugger) {
  // Provide lldb.{debugger,target,process} in the interactive interpreter
  // FIXME: the `var debugger = debugger` are to workaround Swift C++ interop
  // shortcomings. They should get removed.
  auto script_template = R"(
import LLDB

public extension lldb {
    static var debugger: lldb.SBDebugger {
        get { UnsafePointer<lldb.SBDebugger>(bitPattern:0x%llx)!.pointee }
    }
    // GetSelectedTarget() is not const and lldb.debugger is immutable so
    // we need to copy it.
    static var target: lldb.SBTarget {
        get { var debugger = debugger; return debugger.GetSelectedTarget() }
    }
    // GetProcess() is not const and lldb.target is immutable so we need to
    // copy it.
    static var process: lldb.SBProcess {
        get { var target = target; return target.GetProcess() }
    }
}

#sourceLocation(file: "<Swift script prompt>", line: 1)
%s
)";

  SBDebugger sb_debugger(debugger.shared_from_this());

  std::string script = llvm::to_string(
      llvm::format(script_template, &sb_debugger, line.data()));
  std::unique_ptr<llvm::MemoryBuffer> expr_buffer(
      llvm::MemoryBuffer::getMemBufferCopy(script,
                                           "<LLDB prompt command>"));

  return executeMemoryBuffer(std::move(expr_buffer));
}

/// JIT and run the top-level code provided in the MemoryBuffer.
/// This will create a new Swift module and a new JITDylib and make sure
/// previous definitions are available in its context.
Error SwiftInterpreter::executeMemoryBuffer(
    std::unique_ptr<llvm::MemoryBuffer> code) {
  if (auto error = initialize())
    return error;

  auto &ast_context = m_compiler_instance->getASTContext();
  auto &source_manager = m_compiler_instance->getSourceMgr();

  // Make sure previous errors do not prevent us from executing.
  ast_context.Diags.resetHadAnyError();

  unsigned buffer_id = source_manager.addNewSourceBuffer(std::move(code));

  // Make sure previous modules are implicitly imported.
  swift::ImplicitImportInfo import_info;
  import_info.StdlibKind = swift::ImplicitStdlibKind::Stdlib;
  for (auto module : m_modules)
    import_info.AdditionalImports.push_back(module);

  // Create our new module.
  auto module_name = llvm::to_string(
      llvm::formatv("__lldb_swift_script_module_{0}", m_current_id));
  auto module_id = ast_context.getIdentifier(module_name);
  auto *module = swift::ModuleDecl::create(module_id, ast_context, import_info);

  // This puts the compiler in "script" mode.
  swift::SourceFileKind source_file_kind = swift::SourceFileKind::Main;

  // Create the SourceSile for our MemoryBuffer.
  swift::SourceFile *source_file = new (ast_context)
      swift::SourceFile(*module, source_file_kind, buffer_id,
                        swift::SourceFile::ParsingFlags::DisableDelayedBodies);
  module->addFile(*source_file);

  // And compile it...
  swift::performImportResolution(*source_file);
  if (ast_context.hadError())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to perform import resolution");

  swift::bindExtensions(*module);
  if (ast_context.hadError())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to bind extensions");

  swift::performTypeChecking(*source_file);
  if (ast_context.hadError())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Failed to type check");

  // Generate SIL.
  auto sil_types = std::make_unique<swift::Lowering::TypeConverter>(*module);
  std::unique_ptr<swift::SILModule> sil_module = swift::performASTLowering(
      *source_file, *sil_types, m_compiler_invocation->getSILOptions());

  runSILDiagnosticPasses(*sil_module);
  if (ast_context.hadError())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Error during SIL diagnostic passes");
  runSILLoweringPasses(*sil_module);
  if (ast_context.hadError())
    return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                   "Error during SIL lowering");

  // Generate LLVM IR.
  std::unique_ptr<llvm::Module> llvm_module;
  std::unique_ptr<llvm::LLVMContext> llvm_context;
  {
    std::lock_guard<std::recursive_mutex> context_locker(
        IRExecutionUnit::GetLLVMGlobalContextMutex());

    const auto &irgen_opts = m_compiler_invocation->getIRGenOptions();

    auto gen_module = swift::performIRGeneration(
        module, irgen_opts, m_compiler_invocation->getTBDGenOptions(),
        std::move(sil_module), "lldb_module",
        swift::PrimarySpecificPaths("", "<LLDB prompt>"),
        llvm::ArrayRef<std::string>());

    if (!gen_module)
      return llvm::createStringError(llvm::inconvertibleErrorCode(),
                                     "Error generating LLVM IR");

    swift::performLLVMOptimizations(irgen_opts, gen_module.getModule(),
                                    gen_module.getTargetMachine());

    auto context_and_module = std::move(gen_module).release();
    llvm_context.reset(context_and_module.first);
    llvm_module.reset(context_and_module.second);
  }

  // Create a new JITDylib and add the IR to it, compiling it in the process.
  auto maybe_dylib = createJITDylib();
  if (!maybe_dylib)
    return maybe_dylib.takeError();
  auto &dylib = *maybe_dylib;

  auto add_ir_error = m_jit->addIRModule(
      dylib, llvm::orc::ThreadSafeModule(std::move(llvm_module),
                                         std::move(llvm_context)));
  if (add_ir_error)
    return add_ir_error;

  // Given the compiler is in "scripting" mode, it will generate a `main` symbol
  // as an entry point.
  auto maybe_symbol = m_jit->lookup(dylib, llvm::to_string("main"));
  if (!maybe_symbol)
    return maybe_symbol.takeError();

  // Invoke the entry point.
  void (*script)(void) = (void (*)(void))maybe_symbol->getAddress();
  script();

  m_modules.push_back(swift::AttributedImport<swift::ImportedModule>(
      swift::ImportedModule(module)));

  // Remove the entry point so that it doesn't conflict with future ones.
  auto remove_error = dylib.remove({m_jit->mangleAndIntern("main")});
  if (remove_error)
    return remove_error;

  return Error::success();
}
