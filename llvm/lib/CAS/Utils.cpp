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

Error cas::walkFileTreeRecursively(
    CASDB &CAS, CASID ID,
    function_ref<Error(const NamedTreeEntry &, Optional<TreeProxy>)> Callback) {
  BumpPtrAllocator Alloc;
  StringSaver Saver(Alloc);
  SmallString<128> PathStorage;
  SmallVector<NamedTreeEntry> Stack;
  Stack.emplace_back(ID, TreeEntry::Tree, "/");

  while (!Stack.empty()) {
    if (Stack.back().getKind() != TreeEntry::Tree) {
      if (Error E = Callback(Stack.pop_back_val(), None))
        return E;
      continue;
    }

    NamedTreeEntry Parent = Stack.pop_back_val();
    Expected<TreeProxy> ExpTree = CAS.getTree(Parent.getID());
    if (Error E = ExpTree.takeError())
      return E;
    TreeProxy Tree = *ExpTree;
    if (Error E = Callback(Parent, Tree))
      return E;
    for (int I = Tree.size(), E = 0; I != E; --I) {
      Optional<NamedTreeEntry> Child = Tree.get(I - 1);
      assert(Child && "Expected no corruption");

      SmallString<128> PathStorage = Parent.getName();
      sys::path::append(PathStorage, sys::path::Style::posix, Child->getName());
      Stack.emplace_back(Child->getID(), Child->getKind(),
                         Saver.save(StringRef(PathStorage)));
    }
  }

  return Error::success();
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
  OS << " " << getID() << " " << Name;
  if (getKind() == TreeEntry::Tree)
    OS << "/";
  if (getKind() == TreeEntry::Symlink) {
    auto Target = cantFail(CAS.getBlob(getID()));
    OS << " -> " << *Target;
  }
  OS << "\n";
}
