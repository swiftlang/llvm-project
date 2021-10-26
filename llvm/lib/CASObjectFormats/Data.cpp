//===- CASObjectFormats/Data.cpp ------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CASObjectFormats/Data.h"
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/Support/EndianStream.h"

using namespace llvm;
using namespace llvm::casobjectformats;
using namespace llvm::casobjectformats::data;

void FixupList::encode(ArrayRef<Fixup> Fixups, SmallVectorImpl<char> &Data) {
  // FIXME: Kinds should be numbered in a stable way, not just rely on
  // Edge::Kind.
  for (auto &F : Fixups) {
    Data.push_back(static_cast<unsigned char>(F.Kind));
    encoding::writeVBR8(uint64_t(F.Offset), Data);
  }
}

void FixupList::iterator::decode(bool IsInit) {
  if (Data.empty()) {
    assert((IsInit || F) && "past the end");
    F.reset();
    return;
  }

  unsigned char Kind = Data[0];
  Data = Data.drop_front();

  uint64_t Offset = 0;
  bool ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Offset));
  assert(!ConsumeFailed && "Cannot decode vbr8");
  (void)ConsumeFailed;

  F.emplace();
  F->Kind = Kind;
  F->Offset = Offset;
}

void TargetInfoList::encode(ArrayRef<TargetInfo> TIs,
                            SmallVectorImpl<char> &Data) {
  for (const TargetInfo &TI : TIs) {
    assert(TI.Index < TIs.size() && "More targets than edges?");
    encoding::writeVBR8(int64_t(TI.Addend), Data);
    encoding::writeVBR8(uint32_t(TI.Index), Data);
  }
}

void TargetInfoList::iterator::decode(bool IsInit) {
  if (Data.empty()) {
    assert((IsInit || TI) && "past the end");
    TI.reset();
    return;
  }

  int64_t Addend = 0;
  bool ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Addend));
  assert(!ConsumeFailed && "Cannot decode addend");
  (void)ConsumeFailed;

  uint32_t Index = 0;
  ConsumeFailed = errorToBool(encoding::consumeVBR8(Data, Index));
  assert(!ConsumeFailed && "Cannot decode index");
  (void)ConsumeFailed;

  TI.emplace();
  TI->Addend = Addend;
  TI->Index = Index;
}
