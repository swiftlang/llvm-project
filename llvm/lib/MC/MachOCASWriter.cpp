//===- lib/MC/MachOCASWriter.cpp - Mach-O CAS File Writer -----------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/iterator_range.h"
#include "llvm/BinaryFormat/MachO.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/Utils.h"
#include "llvm/CASObjectFormats/CASObjectReader.h"
#include "llvm/CASObjectFormats/FlatV1.h"
#include "llvm/CASObjectFormats/LinkGraph.h"
#include "llvm/CASObjectFormats/NestedV1.h"
#include "llvm/CASObjectFormats/Utils.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"
#include "llvm/ExecutionEngine/JITLink/MachO_x86_64.h"
#include "llvm/MC/MCAsmBackend.h"
#include "llvm/MC/MCAsmLayout.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCDirectives.h"
#include "llvm/MC/MCExpr.h"
#include "llvm/MC/MCFixupKindInfo.h"
#include "llvm/MC/MCFragment.h"
#include "llvm/MC/MCMachOCASWriter.h"
#include "llvm/MC/MCObjectWriter.h"
#include "llvm/MC/MCSection.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCSymbol.h"
#include "llvm/MC/MCSymbolMachO.h"
#include "llvm/MC/MCValue.h"
#include "llvm/Support/Alignment.h"
#include "llvm/Support/Allocator.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/Debug.h"
#include "llvm/Support/ErrorHandling.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::casobjectformats;

#define DEBUG_TYPE "mc"

MachOCASWriter::MachOCASWriter(std::unique_ptr<MCMachObjectTargetWriter> MOTW,
                               cas::CASDB &CAS, CASObjectFormat Format,
                               raw_pwrite_stream &OS, bool IsLittleEndian)
    : CAS(CAS), Format(Format), ObjectOS(ObjectData), OS(OS),
      MOW(std::move(MOTW), ObjectOS, IsLittleEndian) {}

uint64_t MachOCASWriter::writeObject(MCAssembler &Asm,
                                     const MCAsmLayout &Layout) {
  MOW.writeObject(Asm, Layout);

  uint64_t StartOffset = OS.tell();
  if (Format == CASObjectFormat::Native) {
    auto Ref = cantFail(CAS.createBlob(ObjectData.str()));
    writeCASIDBuffer(CAS, Ref.getID(), OS);
    return OS.tell() - StartOffset;
  }

  std::unique_ptr<SchemaBase> Schema;
  switch (Format) {
  case CASObjectFormat::NestedV1:
    Schema = std::make_unique<nestedv1::ObjectFileSchema>(CAS);
    break;
  case CASObjectFormat::FlatV1:
    Schema = std::make_unique<flatv1::ObjectFileSchema>(CAS);
    break;
  default:
    llvm_unreachable("unhandled format");
  }

  auto Obj = MemoryBuffer::getMemBuffer(ObjectData.str());
  auto G = cantFail(jitlink::createLinkGraphFromObject(Obj->getMemBufferRef()));
  auto Ref = cantFail(Schema->createFromLinkGraph(*G));

  writeCASIDBuffer(CAS, Ref.getID(), OS);
  return OS.tell() - StartOffset;
}

std::unique_ptr<MCObjectWriter>
llvm::createMachOCASWriter(std::unique_ptr<MCMachObjectTargetWriter> MOTW,
                           cas::CASDB &CAS, CASObjectFormat Format,
                           raw_pwrite_stream &OS, bool IsLittleEndian) {
  return std::make_unique<MachOCASWriter>(std::move(MOTW), CAS, Format, OS,
                                          IsLittleEndian);
}
