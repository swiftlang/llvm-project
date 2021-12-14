//===- UniqueID.cpp ---------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/UniqueID.h"
#include "llvm/Support/Debug.h"

using namespace llvm;
using namespace llvm::cas;

LLVM_DUMP_METHOD void UniqueIDRef::dump() const { print(dbgs()); }

void UniqueID::print(raw_ostream &OS) const {
  if (isValid())
    UniqueIDRef(*this).print(OS);
  else
    OS << "<none>";
}

LLVM_DUMP_METHOD void UniqueID::dump() const { print(dbgs()); }
