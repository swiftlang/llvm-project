//===- CASOptionsTest.cpp -------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASOptions.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Testing/Support/Error.h"
#include "llvm/Testing/Support/SupportHelpers.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace llvm::cas;

namespace {

TEST(CASOptionsTest, getKind) {
  CASOptions Opts;
  EXPECT_EQ(CASOptions::InMemoryCAS, Opts.getKind());

  Opts.CASPath = "auto";
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  EXPECT_EQ(CASOptions::OnDiskCAS, Opts.getKind());

  Opts.CASPath = Dir.path("cas").str().str();
  EXPECT_EQ(CASOptions::OnDiskCAS, Opts.getKind());
}

TEST(CASOptionsTest, getOrCreateCAS) {
  // Create an in-memory CAS.
  CASOptions Opts;
  std::shared_ptr<cas::CASDB> InMemory = cantFail(Opts.getOrCreateCAS());
  ASSERT_TRUE(InMemory);
  EXPECT_EQ(InMemory, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::InMemoryCAS, Opts.getKind());

  // Create an on-disk CAS.
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  Opts.CASPath = Dir.path("cas").str().str();
  std::shared_ptr<cas::CASDB> OnDisk = cantFail(Opts.getOrCreateCAS());
  EXPECT_NE(InMemory, OnDisk);
  EXPECT_EQ(OnDisk, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::OnDiskCAS, Opts.getKind());

  // Create an on-disk CAS at an automatic location.
  Opts.CASPath = "auto";
  std::shared_ptr<cas::CASDB> OnDiskAuto = cantFail(Opts.getOrCreateCAS());
  EXPECT_NE(InMemory, OnDiskAuto);
  EXPECT_NE(OnDisk, OnDiskAuto);
  EXPECT_EQ(OnDiskAuto, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::OnDiskCAS, Opts.getKind());

  // Create another in-memory CAS. It won't be the same one.
  Opts.CASPath = "";
  std::shared_ptr<cas::CASDB> InMemory2 = cantFail(Opts.getOrCreateCAS());
  EXPECT_NE(InMemory, InMemory2);
  EXPECT_NE(OnDisk, InMemory2);
  EXPECT_NE(OnDiskAuto, InMemory2);
  EXPECT_EQ(InMemory2, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::InMemoryCAS, Opts.getKind());
}

TEST(CASOptionsTest, getOrCreateCASInvalid) {
  // Create a file, then try to put a CAS there.
  StringRef Contents = "contents";
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  unittest::TempFile File(Dir.path("cas"), /*Suffix=*/"",
                          /*Contents=*/Contents);

  CASOptions Opts;
  Opts.CASPath = File.path().str();
  EXPECT_THAT_EXPECTED(Opts.getOrCreateCAS(), Failed());

  auto Empty = cantFail(Opts.getOrCreateCAS(/*CreateEmptyCASOnFailure=*/true));
  EXPECT_EQ(Empty, cantFail(Opts.getOrCreateCAS()));

  // Ensure the file wasn't clobbered.
  std::unique_ptr<MemoryBuffer> MemBuffer;
  ASSERT_THAT_ERROR(
      errorOrToExpected(MemoryBuffer::getFile(File.path())).moveInto(MemBuffer),
      Succeeded());
  ASSERT_EQ(Contents, MemBuffer->getBuffer());
}

TEST(CASOptionsTest, getOrCreateCASAndHideConfig) {
  // Hide the CAS configuration when creating it.
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  CASOptions Opts;
  Opts.CASPath = Dir.path("cas").str().str();
  std::shared_ptr<cas::CASDB> CAS =
      cantFail(Opts.getOrCreateCASAndHideConfig());
  ASSERT_TRUE(CAS);
  EXPECT_EQ(CASOptions::UnknownCAS, Opts.getKind());

  // Check that the configuration is hidden, but calls to getOrCreateCAS()
  // still return the original CAS.
  EXPECT_EQ(CAS->getHashSchemaIdentifier(), Opts.CASPath);

  // Check that new paths are ignored.
  Opts.CASPath = "";
  EXPECT_EQ(CAS, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::UnknownCAS, Opts.getKind());
  EXPECT_EQ(CAS, cantFail(Opts.getOrCreateCASAndHideConfig()));
  EXPECT_EQ(CASOptions::UnknownCAS, Opts.getKind());

  Opts.CASPath = Dir.path("ignored-cas").str().str();
  EXPECT_EQ(CAS, cantFail(Opts.getOrCreateCAS()));
  EXPECT_EQ(CASOptions::UnknownCAS, Opts.getKind());
  EXPECT_EQ(CAS, cantFail(Opts.getOrCreateCASAndHideConfig()));
  EXPECT_EQ(CASOptions::UnknownCAS, Opts.getKind());
}

} // end namespace
