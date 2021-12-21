//===- ActionDescription.cpp ------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/ActionDescription.h"
#include "llvm/CAS/ActionCache.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/EndianStream.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;

char ActionDescriptionError::ID = 0;
void ActionDescriptionError::anchor() {}

void ActionDescriptionError::log(raw_ostream &OS) const {
  ECError::log(OS);
  OS << ": action " << Action;
}

void ActionDescriptionError::saveAction(const ActionDescription &Action) {
  SmallString<1024> Printed;
  raw_svector_ostream(Printed) << Action;
  this->Action = Printed.str().str();
}

static ArrayRef<uint8_t> toArrayRef(StringRef String) {
  return ArrayRef<uint8_t>(reinterpret_cast<const uint8_t *>(String.begin()),
                           reinterpret_cast<const uint8_t *>(String.end()));
}

void ActionDescription::serialize(
    function_ref<void(ArrayRef<uint8_t>)> AppendBytes) const {
  const Namespace *NS = nullptr;
  // Serialize sizes of strings / arrays.
  {
    SmallString<sizeof(uint32_t) * 4> Sizes;
    raw_svector_ostream SizesOS(Sizes);
    support::endian::Writer SizesEW(SizesOS, support::endianness::little);
    SizesEW.write(uint32_t(IDs.size()));
    SizesEW.write(uint32_t(Identifier.size()));
    SizesEW.write(uint32_t(ExtraData.size()));

    if (!IDs.empty()) {
      NS = &IDs.front().getNamespace();

      // FIXME: Consider removing, since the namespace is supposed to be
      // implied from context. But the slight redundancy is cheap and might be
      // useful.
      SizesEW.write(uint32_t(NS->getHashSize()));
    }

    AppendBytes(toArrayRef(Sizes));
  }

  // Serialize content of strings / arrays / hashes.
  AppendBytes(toArrayRef(Identifier));
  for (UniqueIDRef ID : IDs) {
    assert(&ID.getNamespace() == NS && "Expected IDs to share a namespace!");
    AppendBytes(ID.getHash());
  }
  AppendBytes(toArrayRef(ExtraData));
}

void ActionDescription::print(raw_ostream &OS, bool PrintIDs,
                              bool PrintExtraData) const {
  OS << Identifier;
  if (!IDs.empty()) {
    if (!PrintIDs)
      OS << "\n- ids: (count=" << IDs.size() << ")";
    else
      for (UniqueIDRef ID : IDs)
        OS << "\n- id: " << ID;
  }

  if (ExtraData.empty())
    return;

  OS << "\n- extra-data:";
  if (!PrintExtraData)
    OS << " (size=" << ExtraData.size() << ")";
  else
    OS << "\n"
       << format_bytes(toArrayRef(ExtraData), /*FirstByteOffset=*/None,
                       /*NumPerLine=*/32);
}

LLVM_DUMP_METHOD void ActionDescription::dump() const {
  print(dbgs(), /*PrintIDs=*/true, /*PrintExtraData=*/true);
}
