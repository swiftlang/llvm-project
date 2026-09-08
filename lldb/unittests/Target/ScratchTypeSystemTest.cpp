//===-- TestTypeSystem.cpp -------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/Platform/MacOSX/PlatformMacOSX.h"
#include "Plugins/Platform/MacOSX/PlatformRemoteMacOSX.h"
#include "Plugins/TypeSystem/Clang/TypeSystemClang.h"
#include "TestingSupport/SubsystemRAII.h"
#include "TestingSupport/TestUtilities.h"
#include "lldb/Core/Debugger.h"
#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/FileSystem.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Symbol/Type.h"
#include "lldb/Target/Target.h"
#include "lldb/lldb-enumerations.h"
#include "llvm/ADT/ScopeExit.h"
#include "llvm/Testing/Support/Error.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using namespace lldb;
using namespace lldb_private;

class TestTypeSystemMap : public testing::Test {
public:
  SubsystemRAII<FileSystem, HostInfo, PlatformMacOSX> subsystems;

protected:
  void SetUp() override {
    std::call_once(TestUtilities::g_debugger_initialize_flag,
                   []() { Debugger::Initialize(nullptr); });
  };

  DebuggerSP m_debugger_sp;
  PlatformSP m_platform_sp;
};

TEST_F(TestTypeSystemMap, GetScratchTypeSystemForLanguage) {
  // Set up the debugger, make sure that was done properly.
  TargetSP target_sp;
  ArchSpec arch("x86_64-apple-macosx-");
  Platform::SetHostPlatform(PlatformRemoteMacOSX::CreateInstance(true, &arch));

  m_debugger_sp = Debugger::CreateInstance();

  auto &target = m_debugger_sp->GetDummyTarget();
  EXPECT_THAT_EXPECTED(
      target.GetScratchTypeSystemForLanguage(lldb::eLanguageTypeMipsAssembler),
      llvm::FailedWithMessage("No expression support for any languages"));
  EXPECT_THAT_EXPECTED(
      target.GetScratchTypeSystemForLanguage(lldb::eLanguageTypeAssembly),
      llvm::FailedWithMessage("No expression support for any languages"));
  EXPECT_THAT_EXPECTED(
      target.GetScratchTypeSystemForLanguage(lldb::eLanguageTypeUnknown),
      llvm::FailedWithMessage("No expression support for any languages"));
  EXPECT_THAT_EXPECTED(
      target.GetScratchTypeSystemForLanguage(lldb::eLanguageTypeModula2),
      llvm::FailedWithMessage("TypeSystem for language modula2 doesn't exist"));
}

namespace {
/// A TypeSystem that claims only the single language it was assigned, so that
/// registering two of them yields two distinct scratch instances.
class MockTypeSystem : public TypeSystemClang {
public:
  MockTypeSystem()
      : TypeSystemClang("mock", llvm::Triple("x86_64-apple-macosx")) {}

  bool SupportsLanguage(lldb::LanguageType language) override {
    return language == m_language;
  }

  void SetLanguage(lldb::LanguageType language) { m_language = language; }

private:
  lldb::LanguageType m_language = lldb::eLanguageTypeUnknown;
};

/// Fortran77 (0x0007) and Fortran90 (0x0008) are adjacent in LanguageType order
/// and neither is a language TypeSystemClang claims, so the two mocks below are
/// the only TypeSystems in play.
lldb::TypeSystemSP g_fortran77_type_system;
lldb::TypeSystemSP g_fortran90_type_system;

/// TypeSystem::CreateInstance calls every registered callback in turn, so each
/// one has to decline the languages it doesn't handle.
lldb::TypeSystemSP CreateFortran77TypeSystem(lldb::LanguageType language,
                                             Module *, Target *) {
  return language == lldb::eLanguageTypeFortran77 ? g_fortran77_type_system
                                                  : lldb::TypeSystemSP();
}

lldb::TypeSystemSP CreateFortran90TypeSystem(lldb::LanguageType language,
                                             Module *, Target *) {
  return language == lldb::eLanguageTypeFortran90 ? g_fortran90_type_system
                                                  : lldb::TypeSystemSP();
}
} // namespace

TEST_F(TestTypeSystemMap, GetScratchTypeSystemsIsOrderedByLanguage) {
  ArchSpec arch("x86_64-apple-macosx-");
  Platform::SetHostPlatform(PlatformRemoteMacOSX::CreateInstance(true, &arch));

  // Assign Fortran90 to whichever instance ends up at the lower address (the
  // allocator gives no guarantee about the relative order of two make_shared
  // calls, so this has to be measured rather than assumed). The order in
  // memory is then the reverse of the order of the languages, and an
  // implementation that deduplicates by sorting on pointer identity returns
  // the two the wrong way round.
  auto ts_a = std::make_shared<MockTypeSystem>();
  auto ts_b = std::make_shared<MockTypeSystem>();
  if (ts_a.get() > ts_b.get())
    std::swap(ts_a, ts_b);
  ASSERT_LT(ts_a.get(), ts_b.get());

  ts_a->SetLanguage(lldb::eLanguageTypeFortran90);
  ts_b->SetLanguage(lldb::eLanguageTypeFortran77);
  g_fortran90_type_system = ts_a;
  g_fortran77_type_system = ts_b;

  LanguageSet fortran77;
  fortran77.Insert(lldb::eLanguageTypeFortran77);
  LanguageSet fortran90;
  fortran90.Insert(lldb::eLanguageTypeFortran90);
  ASSERT_TRUE(PluginManager::RegisterPlugin(
      "mock-fortran77", "", CreateFortran77TypeSystem, fortran77, fortran77));
  ASSERT_TRUE(PluginManager::RegisterPlugin(
      "mock-fortran90", "", CreateFortran90TypeSystem, fortran90, fortran90));
  llvm::scope_exit unregister([]() {
    PluginManager::UnregisterPlugin(CreateFortran77TypeSystem);
    PluginManager::UnregisterPlugin(CreateFortran90TypeSystem);
    g_fortran77_type_system.reset();
    g_fortran90_type_system.reset();
  });

  m_debugger_sp = Debugger::CreateInstance();
  auto &target = m_debugger_sp->GetDummyTarget();

  EXPECT_THAT(
      target.GetScratchTypeSystems(),
      testing::ElementsAre(g_fortran77_type_system, g_fortran90_type_system));
}
