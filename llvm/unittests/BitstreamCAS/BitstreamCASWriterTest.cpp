//===- BitstreamCASWriterTest.cpp - Tests for BitstreamWriter
//----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/BitstreamCAS/BitstreamCASWriter.h"
#include "llvm/Bitstream/BitstreamWriter.h"
#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"
#include "llvm/Support/Error.h"
#include "gtest/gtest.h"
#include <iostream>
#include <optional>

using namespace llvm;

namespace {

// Test fixture for BitstreamCASWriter tests
class BitstreamCASWriterTest : public ::testing::Test {
protected:
  const unsigned BlkID = bitc::FIRST_APPLICATION_BLOCKID + 17;

  std::shared_ptr<cas::ObjectStore> CAS;

  void SetUp() override { CAS = cas::createInMemoryCAS(); }
};

TEST_F(BitstreamCASWriterTest, EmitBlob) {
  BitstreamCASWriter Writer(CAS);
  std::string InputBlob = "str";

  Writer.emitBlob(InputBlob, /*ShouldEmitSize=*/false);

  std::optional<cas::CASID> RootCASID;
  cantFail(Writer.EndStreaming().moveInto(RootCASID));
  ASSERT_TRUE(RootCASID.has_value());

  std::optional<cas::ObjectProxy> RootObjectProxy;
  cantFail(CAS->getProxy(*RootCASID).moveInto(RootObjectProxy));
  ASSERT_TRUE(RootObjectProxy.has_value());

  EXPECT_EQ(InputBlob, RootObjectProxy->getData().data());
  EXPECT_EQ(0UL, RootObjectProxy->getNumReferences());
}

TEST_F(BitstreamCASWriterTest, EmitBlobEmpty) {
  BitstreamCASWriter Writer(CAS);

  Writer.emitBlob("", /*ShouldEmitSize=*/false);

  std::optional<cas::CASID> RootCASID;
  cantFail(Writer.EndStreaming().moveInto(RootCASID));
  ASSERT_TRUE(RootCASID.has_value());

  std::optional<cas::ObjectProxy> RootObjectProxy;
  cantFail(CAS->getProxy(*RootCASID).moveInto(RootObjectProxy));
  ASSERT_TRUE(RootObjectProxy.has_value());

  EXPECT_EQ(0UL, RootObjectProxy->getData().size());
  EXPECT_EQ(0UL, RootObjectProxy->getNumReferences());
}

TEST_F(BitstreamCASWriterTest, EmitBlob4ByteAligned) {
  BitstreamCASWriter Writer(CAS);
  std::string InputBlob = "str0";

  Writer.emitBlob(InputBlob, /*ShouldEmitSize=*/false);

  std::optional<cas::CASID> RootCASID;
  cantFail(Writer.EndStreaming().moveInto(RootCASID));
  ASSERT_TRUE(RootCASID.has_value());

  std::optional<cas::ObjectProxy> RootObjectProxy;
  cantFail(CAS->getProxy(*RootCASID).moveInto(RootObjectProxy));
  ASSERT_TRUE(RootObjectProxy.has_value());

  EXPECT_EQ(InputBlob, RootObjectProxy->getData().data());
  EXPECT_EQ(0UL, RootObjectProxy->getNumReferences());
}

TEST_F(BitstreamCASWriterTest, EmitBlobWithSize) {
  BitstreamCASWriter Writer(CAS);
  std::string InputBlob = "str";

  Writer.emitBlob(InputBlob);

  std::optional<cas::CASID> RootCASID;
  cantFail(Writer.EndStreaming().moveInto(RootCASID));
  ASSERT_TRUE(RootCASID.has_value());

  std::optional<cas::ObjectProxy> RootObjectProxy;
  cantFail(CAS->getProxy(*RootCASID).moveInto(RootObjectProxy));
  ASSERT_TRUE(RootObjectProxy.has_value());

  // NOTE: Verify the data content of the object proxy using BitstreamWriter.
  // Will be replaced using BitstreamCASReader in the future.
  SmallString<64> Expected;
  {
    BitstreamWriter W(Expected);
    W.EmitVBR(3, 6);
    W.FlushToWord();
    W.Emit('s', 8);
    W.Emit('t', 8);
    W.Emit('r', 8);
    W.Emit(0, 8);
  }
  EXPECT_EQ(Expected.str(), RootObjectProxy->getData());
  EXPECT_EQ(0UL, RootObjectProxy->getNumReferences());
}

TEST_F(BitstreamCASWriterTest, EmitBlock) {
  BitstreamCASWriter Writer(CAS);
  Writer.EnterSubblock(BlkID, 2);
  Writer.EmitVBR(42, 2);
  Writer.ExitBlock();

  std::optional<cas::CASID> RootCASID;
  cantFail(Writer.EndStreaming().moveInto(RootCASID));
  ASSERT_TRUE(RootCASID.has_value());

  std::optional<cas::ObjectProxy> RootObjectProxy;
  cantFail(CAS->getProxy(*RootCASID).moveInto(RootObjectProxy));
  ASSERT_TRUE(RootObjectProxy.has_value());

  EXPECT_EQ(0UL, RootObjectProxy->getData()
                     .size()); // No data will be written to the root object as
                               // it has no emits before EnterSubblock.
  EXPECT_EQ(1UL, RootObjectProxy->getNumReferences());

  std::optional<cas::ObjectProxy> BlockObjectProxy;
  cantFail(CAS->getProxy(RootObjectProxy->getReference(0))
               .moveInto(BlockObjectProxy));
  ASSERT_TRUE(BlockObjectProxy.has_value());
  EXPECT_EQ(0UL, BlockObjectProxy->getNumReferences());
  EXPECT_LT(0UL, BlockObjectProxy->getData()
                     .size()); // NOTE: Verifying the exact data will be done
                               // using BitstreamCASReader in the future.
}

// TODO: Add more complex tests once the BitstreamCASReader is implemented.

} // end namespace
