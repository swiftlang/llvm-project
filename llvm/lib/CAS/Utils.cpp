//===- Utils.cpp ------------------------------------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/Utils.h"
#include "llvm/BinaryFormat/Magic.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/TreeSchema.h"
// FIXME: It's layer violation including this (libLLVMCAS should not depend on
// headers of libLLVMCASObjectFormats) but 'Encoding.h' itself should really
// move to libLLVMSupport.
#include "llvm/CASObjectFormats/Encoding.h"
#include "llvm/Support/MemoryBufferRef.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/StringSaver.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casobjectformats;

Expected<CASID> cas::readCASIDBuffer(cas::CASDB &CAS, MemoryBufferRef Buffer) {
  if (identify_magic(Buffer.getBuffer()) != file_magic::cas_id)
    return createStringError(std::errc::invalid_argument,
                             "buffer does not contain a CASID");

  StringRef Remaining =
      Buffer.getBuffer().substr(StringRef(casidObjectMagicPrefix).size());
  uint32_t Size;
  if (auto E = encoding::consumeVBR8(Remaining, Size))
    return std::move(E);

  StringRef CASIDStr = Remaining.substr(0, Size);
  return CAS.parseID(CASIDStr);
}

void cas::writeCASIDBuffer(const CASID &ID, llvm::raw_ostream &OS) {
  OS << casidObjectMagicPrefix;
  SmallString<256> CASIDStr;
  raw_svector_ostream(CASIDStr) << ID;

  // Write out the size of the CASID so that we can read it back properly even
  // if the buffer has additional padding (e.g. after getting added in an
  // archive).
  SmallString<2> SizeBuf;
  encoding::writeVBR8(CASIDStr.size(), SizeBuf);
  OS << SizeBuf << CASIDStr;
}

static void printTreeEntryKind(raw_ostream &OS, TreeEntry::EntryKind Kind) {
  switch (Kind) {
  case TreeEntry::Regular:
    OS << "file";
    break;
  case TreeEntry::Executable:
    OS << "exec";
    break;
  case TreeEntry::Symlink:
    OS << "syml";
    break;
  case TreeEntry::Tree:
    OS << "tree";
    break;
  }
}

void cas::NamedTreeEntry::print(raw_ostream &OS, CASDB &CAS) const {
  printTreeEntryKind(OS, getKind());
  OS << " " << CAS.getID(getRef()) << " " << Name;
  if (getKind() == TreeEntry::Tree)
    OS << "/";
  if (getKind() == TreeEntry::Symlink) {
    ObjectProxy Target = cantFail(CAS.getProxy(getRef()));
    OS << " -> ";
    OS << Target.getData();
  }
  OS << "\n";
}
