//===- OutputConfig.cpp - Configure compiler outputs ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/Support/OutputConfig.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::vfs;

void OutputConfig::print(raw_ostream &OS) const {
  OS << "{";
  bool IsFirst = true;
  auto printFlag = [&](StringRef FlagName, bool Value) {
    if (IsFirst)
      IsFirst = false;
    else
      OS << ",";
    if (!Value)
      OS << "No";
    OS << FlagName;
  };

#define HANDLE_OUTPUT_CONFIG_FLAG(NAME, DEFAULT)                               \
  if (get##NAME() != DEFAULT)                                                  \
    printFlag(#NAME, get##NAME());
#include "llvm/Support/OutputConfig.def"
  OS << "}";
}

LLVM_DUMP_METHOD void OutputConfig::dump() const { print(dbgs()); }
