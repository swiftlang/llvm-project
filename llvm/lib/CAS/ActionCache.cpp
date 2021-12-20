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

char ActionDescriptionError::ID = 0;
void ActionDescriptionError::anchor() {}

void ActionDescriptionError::log(raw_ostream &OS) const override {
  ECError::log(OS);
  OS << ": action " << Action;
}

void ActionDescriptionError::saveAction(const ActionDescription &Action) {
  SmallString<128> Printed;
  raw_svector_stream(Printed) << Action;
  this->Action = Printed;
}

char ActionCacheCollisionError::ID = 0;
void ActionCacheCollisionError::anchor() {}

void ActionCacheCollisionError::log(raw_ostream &OS) const {
  OS << "cache collision: result " << ExistingResult << " exists; ignoring "
     << NewResult << " for action " << getAction();
}

char CorruptActionCacheResultError::ID = 0;
void CorruptActionCacheResultError::anchor() {}

void CorruptActionCacheResultError::log(raw_ostream &OS) const override {
  OS << "cached result corrupt for action " << getAction();
}

char WrongActionCacheNamespaceError::ID = 0;
void WrongActionCacheNamespaceError::anchor() {}

void WrongActionCacheNamespaceError::log(raw_ostream &OS) const {
  OS << "wrong namespace for action cache";
  if (Path)
    OS << "at '" << *Path << "'";
  OS << ": expected '" << ExpectedNamespace << " but observed '"
     << ObservedNamespace << ")";
}
static ArrayRef<uint8_t> toArrayRef(StringRef String) {
  return ArrayRef<uint8_t>(static_cast<const uint8_t *>(String.begin()),
                           static_cast<const uint8_t *>(String.end()));
}

void ActionDescription::serialize(function_ref<void (ArrayRef<uint8_t>)> AppendBytes) const {
  const Namespace *NS = nullptr;
  // Serialize sizes of all strings / arrays / hashes.
  {
    SmallVector<char, 64> Sizes;
    raw_svector_ostream SizesOS(Sizes);
    support::endian::Writer SizesEW(SizesOS, support::endianness::little);
    SizesEW.write(uint32_t(Identifier.size()));
    SizesEW.write(uint32_t(ExtraData.size()));
    SizesEW.write(uint32_t(IDs.size()));

    if (!IDs.empty()) {
      NS = &IDs.front().getNamespace();
      SizesEW.write(uint32_t(NS->getHashSize()));
      SizesEW.write(uint32_t(NS->getName().size()));
    }

    AppendBytes(Sizes);
  }

  // Serialize content of strings / arrays / hashes.
  AppendBytes(toArrayRef(Identifier));
  if (NS)
    AppendBytes(toArrayRef(NS->getName()));
  for (ObjectIDRef ID : ID) {
    assert(&ID.getNamespace() == NS && "Expected IDs to share a namespace!");
    AppendBytes(ID.getHash());
  }
  AppendBytes(toArrayRef(ExtraData));
}

void ActionDescription::print(raw_ostream &OS, bool PrintIDs, bool PrintExtraData) const {
  OS << Identifier << "\n";
  if (!IDs.empty()) {
    if (!PrintIDs)
      OS << "- ids: (count=" << IDs.size() << ")\n";
    else
      for (UniqueIDRef ID : IDs)
        OS << "- id: " << ID << "\n";
  }

  if (ExtraData.empty())
    return;

  OS << "- extra-data:";
  if (!PrintExtraData)
    OS << " (size=" << ExtraData.size() << ")\n";
  else
    OS << "\n"
       << format_bytes(toArrayRef(ExtraData), /*FirstByteOffset=*/None, /*NumPerLine=*/32);
}

LLVM_DUMP_METHOD void ActionDescription::dump() const { print(dbgs(), /*PrintIDs=*/true, /*PrintExtraData=*/true); }

void ActionCache::anchor() {}
