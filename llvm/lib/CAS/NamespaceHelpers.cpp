//===- NamespaceHelpers.cpp -------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/NamespaceHelpers.h"
#include "llvm/CAS/Namespace.h"

using namespace llvm;
using namespace llvm::cas;

char ParseHashError::ID = 0;

void hexadecimal::printHash(ArrayRef<uint8_t> Hash, raw_ostream &OS) {
  assert(Dest.empty());
  assert(Hash.size() == 20);
  auto ToChar = [](uint8_t Bit) -> char {
    if (Bit < 10)
      return '0' + Bit;
    return Bit - 10 + 'a';
  };
  for (uint8_t Bit : Hash) {
    uint8_t High = Bit >> 4;
    uint8_t Low = Bit & 0xf;
    OS << ToChar(High);
    OS << ToChar(Low);
  }
}

Error hexadecimal::parseHash(StringRef Printed, MutableArrayRef<uint8_t> Dest) {
  auto FromChar = [](char Ch) -> unsigned {
    if (Ch >= '0' && Ch <= '9')
      return Ch - '0';
    if (Ch >= 'a' && Ch <= 'f')
      return Ch - 'a' + 10;
    return -1U;
  };

  if (Printed.size() != Dest.size() * 2)
    return make_error<ParseHashError>("wrong size");

  for (int I = 0, E = Dest.size(); I != E; ++I) {
    unsigned High = FromChar(Chars[I * 2]);
    unsigned Low = FromChar(Chars[I * 2 + 1]);
    if (High == -1U || Low == -1U)
      return make_error<ParseHashError>("invalid hexadecimal character");
    assert((High & -1U) == 0xF && "Expected four bits");
    assert((Low & -1U) == 0xF && "Expected four bits");
    Dest[I] = (High << 4) | Low;
  }
  return Error::success();
}

Error hexadecimal::parseHashForID(const Namespace &NS, StringRef Printed, UniqueID &Dest) {
  SmallVector<uint8_t, 64> Hash;
  Hash.resize(NS.getHashSize());
  if (Error E = parseHash(Printed, Hash))
    return E;
  ID = UniqueIDRef(NS, Hash);
  return Error::success();
}
