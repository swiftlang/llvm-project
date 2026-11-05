//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "Plugins/Language/Swift/SwiftLanguage.h"
#include "Plugins/LanguageRuntime/Swift/SwiftLanguageRuntime.h"
#include "TestingSupport/SubsystemRAII.h"
#include "lldb/Core/Mangled.h"
#include "lldb/lldb-enumerations.h"

#include "gtest/gtest.h"

#include <memory>
#include <optional>

using namespace lldb_private;

TEST(SwiftLanguage, RegistersSwiftManglingScheme) {
  SubsystemRAII<SwiftLanguage> langs;

  llvm::StringRef swift_mangled_names[] = {
      "_TtC4main7MyClass",     // Mangled objc class name
      "_TtGC4main7MyClassSi_", // Mangled generic objc class name
      "_TtP4main3Foo_",        // Mangled objc protocol name
      "$s4main3BarCACycfC",    // Mangled name
      "_$s4main3BarCACycfC",   // Mangled name with leading underscore
      "$S4main3BarCACycfC",    // Older swift mangled name
      "_$S4main3BarCACycfC",   // Older swift mangled name
                               // with leading underscore
      "$e4main3BarCACycfC",    // Embedded swift mangled name
      "_$e4main3BarCACycfC",   // Embedded swift mangled name
                               // with leading underscore
      // Mangled swift filename
      "@__swiftmacro_4main16FunVariableNames9OptionSetfMm_.swift",
      // Async main entry point and its funclets
      "async_Main",
      "_async_Main",
      "async_MainTQ0_",
      "_async_MainTQ0_",
      "async_MainTY1_",
      "_async_MainTY1_",
  };
  for (llvm::StringRef name : swift_mangled_names) {
    EXPECT_TRUE(SwiftLanguageRuntime::IsSwiftMangledName(name)) << name;
    EXPECT_EQ(
        Mangled::GetLanguageForManglingScheme(Mangled::GetManglingScheme(name)),
        lldb::eLanguageTypeSwift)
        << name;
    EXPECT_EQ(Mangled(name).GetMangledName(), ConstString(name)) << name;
  }

  llvm::StringRef other_names[] = {"async_main", "_T0foo", "_Tfoo", "$foo"};
  for (llvm::StringRef name : other_names) {
    EXPECT_FALSE(SwiftLanguageRuntime::IsSwiftMangledName(name)) << name;
    EXPECT_EQ(Mangled::GetManglingScheme(name), Mangled::eManglingSchemeNone)
        << name;
  }
}

TEST(SwiftLanguage, DemanglesSwiftNames) {
  SubsystemRAII<SwiftLanguage> langs;

  EXPECT_EQ(Mangled("async_Main").GetDemangledName(),
            ConstString("async main entry point"));

  // Without a function to resolve generic parameters in, the demangled name
  // isn't cached.
  ConstString mangled_name("$s4main3fooyyF");
  Mangled mangled(mangled_name);
  EXPECT_EQ(mangled.GetDemangledName(), ConstString("main.foo() -> ()"));
  ConstString counterpart;
  EXPECT_FALSE(mangled_name.GetMangledCounterpart(counterpart));

  EXPECT_EQ(mangled.GetBaseName(), ConstString("foo"));
  EXPECT_EQ(mangled.GetDisplayDemangledName(), ConstString("foo()"));
}

TEST(SwiftLanguage, ParsesSwiftFunctionNames) {
  SubsystemRAII<SwiftLanguage> langs;

  auto parse =
      [](llvm::StringRef name) -> std::optional<Mangled::FunctionNameParts> {
    Mangled::ManglingScheme scheme = Mangled::GetManglingScheme(name);
    Language *lang = Language::FindPluginForManglingScheme(scheme);
    if (!lang)
      return std::nullopt;
    std::unique_ptr<Language::MangledNameParser> parser =
        lang->CreateMangledNameParser(scheme);
    if (!parser)
      return std::nullopt;
    return parser->Parse(Mangled(name));
  };

  std::optional<Mangled::FunctionNameParts> function = parse("$s4main3fooyyF");
  ASSERT_TRUE(function);
  EXPECT_EQ(function->basename, ConstString("foo"));
  EXPECT_EQ(function->kind, Mangled::FunctionNameKind::InNamespace);

  std::optional<Mangled::FunctionNameParts> method =
      parse("$s4main3BarV3bazyyF");
  ASSERT_TRUE(method);
  EXPECT_EQ(method->basename, ConstString("baz"));
  EXPECT_EQ(method->kind, Mangled::FunctionNameKind::Method);
}
