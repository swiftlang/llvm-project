//===- Namespace.cpp --------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/Namespace.h"
#include "llvm/CAS/UniqueID.h"
#include "llvm/Support/ErrorHandling.h"

using namespace llvm;
using namespace llvm::cas;

void Namespace::anchor() {}

void Namespace::printID(const UniqueIDRef &ID, raw_ostream &OS) const {
  if (&ID.getNamespace() == this) {
    printIDImpl(ID, OS);
    return;
  }

  report_fatal_error("CAS namespace '" + getName() +
                     "' cannot print ID from namespace '" +
                     ID.getNamespace().getName() + "'");
}
