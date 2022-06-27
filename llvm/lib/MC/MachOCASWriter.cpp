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
#include "llvm/MC/CAS/MCCASFormatSchemaBase.h"
#include "llvm/MC/CAS/MCCASObjectV1.h"
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
#include <memory>

using namespace llvm;
using namespace llvm::cas;
using namespace llvm::mccasformats;

#define DEBUG_TYPE "mc"

MachOCASWriter::MachOCASWriter(
    std::unique_ptr<MCMachObjectTargetWriter> MOTW, const Triple &TT,
    cas::CASDB &CAS, CASBackendMode Mode, raw_pwrite_stream &OS,
    bool IsLittleEndian,
    Optional<MCTargetOptions::ResultCallBackTy> ResultCallBack)
    : Target(TT), CAS(CAS), Mode(Mode), ResultCallBack(ResultCallBack), OS(OS),
      InternalOS(InternalBuffer),
      MOW(std::move(MOTW), InternalOS, IsLittleEndian) {
  assert(TT.isLittleEndian() == IsLittleEndian && "Endianess should match");
}

uint64_t MachOCASWriter::writeObject(MCAssembler &Asm,
                                     const MCAsmLayout &Layout) {
  uint64_t StartOffset = OS.tell();
  auto Schema = std::make_unique<v1::MCSchema>(CAS);
  auto CASObj = cantFail(Schema->createFromMCAssembler(*this, Asm, Layout));

  auto VerifyObject = [&]() -> Error {
    SmallString<512> ObjectBuffer;
    raw_svector_ostream ObjectOS(ObjectBuffer);
    if (auto E = Schema->serializeObjectFile(CASObj, ObjectOS))
      return E;

    if (!ObjectBuffer.equals(InternalBuffer))
      return createStringError(
          inconvertibleErrorCode(),
          "CASBackend output round-trip verification error");

    return Error::success();
  };
  // If there is a callback, then just hand off the result through callback.
  if (ResultCallBack) {
    cantFail((*ResultCallBack)(CASObj.getID()));
    if (Mode == CASBackendMode::Verify) {
      if (auto E = VerifyObject())
        report_fatal_error(std::move(E));
    }
    return 0;
  }

  switch (Mode) {
  case CASBackendMode::CASID:
    writeCASIDBuffer(CASObj.getID(), OS);
    break;
  case CASBackendMode::Native: {
    auto E = Schema->serializeObjectFile(CASObj, OS);
    if (E)
      report_fatal_error(std::move(E));
    break;
  }
  case CASBackendMode::Verify: {
    SmallString<512> ObjectBuffer;
    raw_svector_ostream ObjectOS(ObjectBuffer);
    auto E = Schema->serializeObjectFile(CASObj, ObjectOS);
    if (E)
      report_fatal_error(std::move(E));

    if (!ObjectBuffer.equals(InternalBuffer))
      report_fatal_error("CASBackend output round-trip verification error");

    OS << ObjectBuffer;
    break;
  }
  }

  return OS.tell() - StartOffset;
}

std::unique_ptr<MCObjectWriter> llvm::createMachOCASWriter(
    std::unique_ptr<MCMachObjectTargetWriter> MOTW, const Triple &TT,
    cas::CASDB &CAS, CASBackendMode Mode, raw_pwrite_stream &OS,
    bool IsLittleEndian,
    Optional<MCTargetOptions::ResultCallBackTy> ResultCallBack) {
  return std::make_unique<MachOCASWriter>(std::move(MOTW), TT, CAS, Mode, OS,
                                          IsLittleEndian, ResultCallBack);
}
