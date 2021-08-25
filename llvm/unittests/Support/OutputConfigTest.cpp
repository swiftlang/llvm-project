//===- OutputConfigTest.cpp - OutputConfig gets --------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/OutputConfig.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::vfs;

namespace {

TEST(OutputConfigTest, construct) {
  // Test defaults.
  EXPECT_FALSE(OutputConfig().getText());
  EXPECT_FALSE(OutputConfig().getTextWithCRLF());
  EXPECT_FALSE(OutputConfig().getVolatile());
  EXPECT_TRUE(OutputConfig().getCrashCleanup());
  EXPECT_TRUE(OutputConfig().getAtomicWrite());
  EXPECT_TRUE(OutputConfig().getImplyCreateDirectories());
  EXPECT_TRUE(OutputConfig().getOverwrite());

  // Test inverted defaults.
  EXPECT_TRUE(OutputConfig().getNoText());
  EXPECT_TRUE(OutputConfig().getNoTextWithCRLF());
  EXPECT_TRUE(OutputConfig().getNoVolatile());
  EXPECT_FALSE(OutputConfig().getNoCrashCleanup());
  EXPECT_FALSE(OutputConfig().getNoAtomicWrite());
  EXPECT_FALSE(OutputConfig().getNoImplyCreateDirectories());
  EXPECT_FALSE(OutputConfig().getNoOverwrite());
}

TEST(OutputConfigTest, set) {
  // Check a flag that defaults to false. Try both 'get's, all three 'set's,
  // and turning back off after turning it on.
  ASSERT_TRUE(OutputConfig().getNoText());
  EXPECT_TRUE(OutputConfig().setText().getText());
  EXPECT_FALSE(OutputConfig().setText().getNoText());
  EXPECT_TRUE(OutputConfig().setText(true).getText());
  EXPECT_FALSE(OutputConfig().setText().setNoText().getText());
  EXPECT_FALSE(OutputConfig().setText().setText(false).getText());

  // Check a flag that defaults to true. Try both 'get's, all three 'set's, and
  // turning back on after turning it off.
  ASSERT_TRUE(OutputConfig().getCrashCleanup());
  EXPECT_FALSE(OutputConfig().setNoCrashCleanup().getCrashCleanup());
  EXPECT_TRUE(OutputConfig().setNoCrashCleanup().getNoCrashCleanup());
  EXPECT_FALSE(OutputConfig().setCrashCleanup(false).getCrashCleanup());
  EXPECT_TRUE(
      OutputConfig().setNoCrashCleanup().setCrashCleanup().getCrashCleanup());
  EXPECT_TRUE(OutputConfig()
                  .setNoCrashCleanup()
                  .setCrashCleanup(true)
                  .getCrashCleanup());

  // Set multiple flags.
  OutputConfig Config;
  Config.setText().setNoCrashCleanup().setNoImplyCreateDirectories();
  EXPECT_TRUE(Config.getText());
  EXPECT_TRUE(Config.getNoCrashCleanup());
  EXPECT_TRUE(Config.getNoImplyCreateDirectories());
}

TEST(OutputConfigTest, print) {
  auto toString = [](OutputConfig Config) {
    std::string Printed;
    raw_string_ostream OS(Printed);
    Config.print(OS);
    return Printed;
  };
  EXPECT_EQ("{}", toString(OutputConfig()));
  EXPECT_EQ("{Text}", toString(OutputConfig().setText()));
  EXPECT_EQ("{Text,NoCrashCleanup}",
            toString(OutputConfig().setText().setNoCrashCleanup()));
  EXPECT_EQ("{Text,NoCrashCleanup}",
            toString(OutputConfig().setNoCrashCleanup().setText()));
}

} // anonymous namespace
