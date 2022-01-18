//===- ActionCache.cpp ------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionCache.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/raw_ostream.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/Format.h"

using namespace llvm;
using namespace llvm::cas;

char ActionCacheCollisionError::ID = 0;
void ActionCacheCollisionError::anchor() {}

void ActionCacheCollisionError::log(raw_ostream &OS) const {
  OS << "cache collision: result " << ExistingResult << " exists; ignoring "
     << NewResult << " for action " << getAction();
}

char CorruptActionCacheResultError::ID = 0;
void CorruptActionCacheResultError::anchor() {}

void CorruptActionCacheResultError::log(raw_ostream &OS) const {
  OS << "cached result corrupt for action " << getAction();
}

char WrongActionCacheNamespaceError::ID = 0;
void WrongActionCacheNamespaceError::anchor() {}

void WrongActionCacheNamespaceError::log(raw_ostream &OS) const {
  OS << "wrong action cache namespace: expected '" << ExpectedNamespaceName
     << "' but observed '" << ObservedNamespaceName << "'";
}

void ActionCache::anchor() {}
