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

TEST(CASOptionsTest, CASIDFile) {
  // Round trip the content through CASID file and check equal.
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  CASOptions Opts;
  Opts.CASPath = Dir.path("cas").str().str();
  auto CAS = cantFail(Opts.getOrCreateCAS());

  SmallString<256> FileContent;
  raw_svector_ostream SS(FileContent);

  auto Blob = cantFail(CAS->createBlob("blob"));
  Opts.writeCASIDFile(SS, Blob.getID());

  auto Buf = MemoryBuffer::getMemBufferCopy(FileContent);

  CASOptions Opts2;
  auto ID = Opts2.createFromCASIDFile(Buf->getMemBufferRef());
  EXPECT_THAT_EXPECTED(ID, Succeeded());
  EXPECT_EQ(Opts, Opts2);

  auto CAS2 = cantFail(Opts2.getOrCreateCAS());
  auto Blob2 = cantFail(CAS2->getBlob(*ID));
  EXPECT_EQ(Blob.getData(), Blob2.getData());
}

TEST(CASOptionsTest, CASIDFileSwitching) {
  // Test switching CASOptions when reading CASIDFile.
  // Create normal CASOptions.
  unittest::TempDir Dir("cas-options", /*Unique=*/true);
  CASOptions Opts1;
  Opts1.CASPath = Dir.path("cas").str().str();
  auto createInputFile = [](CASOptions &Opts, StringRef Data) {
    SmallString<256> FileContent;
    raw_svector_ostream SS(FileContent);
    auto CAS = cantFail(Opts.getOrCreateCAS());
    auto Blob = cantFail(CAS->createBlob(Data));
    Opts.writeCASIDFile(SS, Blob.getID());
    return FileContent;
  };
  auto readCASFile = [](CASOptions &Opts, SmallString<256> &FileContent) {
    auto Buf = MemoryBuffer::getMemBufferCopy(FileContent);
    return Opts.createFromCASIDFile(Buf->getMemBufferRef());
  };
  auto File1 = createInputFile(Opts1, "blob1");
  auto Blob1 = readCASFile(Opts1, File1);
  EXPECT_THAT_EXPECTED(Blob1, Succeeded());

  // Create frozen CASOptions.
  unittest::TempDir Dir2("cas-options", /*Unique=*/true);
  CASOptions Opts2;
  Opts2.CASPath = Dir2.path("cas").str().str();
  // Freeze Opt2.
  auto CAS2 = cantFail(Opts2.getOrCreateCASAndHideConfig());
  auto File2 = createInputFile(Opts2, "blob2");
  auto Blob2 = readCASFile(Opts2, File2);
  EXPECT_THAT_EXPECTED(Blob2, Succeeded());

  // Use Opts1 to read File2 should work.
  auto Blob3 = readCASFile(Opts1, File2);
  EXPECT_THAT_EXPECTED(Blob3, Succeeded());

  // Use Opts2 to read File1 should fail.
  auto Blob4 = readCASFile(Opts2, File1);
  EXPECT_THAT_EXPECTED(Blob4, Failed());

  // And Opts2 can read the file created by updated Opts1.
  auto File3 = createInputFile(Opts1, "data");
  auto Blob5 = readCASFile(Opts2, File3);
  EXPECT_THAT_EXPECTED(Blob5, Succeeded());
}

} // end namespace
