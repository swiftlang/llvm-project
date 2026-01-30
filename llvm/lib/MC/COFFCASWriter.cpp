//===- lib/MC/COFFCASWriter.cpp - COFF CAS File Writer --------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "llvm/CAS/Utils.h"
#include "llvm/MC/MCWinCOFFCASWriter.h"

namespace llvm {
COFFCASWriter::COFFCASWriter(
    std::unique_ptr<MCWinCOFFObjectTargetWriter> TW, const Triple &Target,
    cas::ObjectStore &CAS, CASBackendMode CBM, raw_pwrite_stream &OS,
    std::function<const cas::ObjectProxy(llvm::MCObjectWriter &,
                                         llvm::MCAssembler &,
                                         cas::ObjectStore &, raw_ostream *)>
        Create,
    std::function<Error(cas::ObjectProxy, cas::ObjectStore &, raw_ostream &)>
        Serialize,
    std::optional<MCTargetOptions::ResultCallBackTy> CallBack,
    raw_pwrite_stream *CASIDOS)
    : WinCOFFObjectWriter(std::move(TW), InternalOS), Target(Target), CAS(CAS),
      Mode(CBM), CreateFromMCAssembler(Create), SerializeObjectFile(Serialize),
      ResultCallBack(CallBack), OS(OS), CASIDOS(CASIDOS),
      InternalOS(InternalBuffer) {}

Error COFFCASWriter::VerifyObject(cas::ObjectProxy CASObject) {
  SmallString<512> ObjectBuffer;
  raw_svector_ostream ObjectOS(ObjectBuffer);
  if (auto E = SerializeObjectFile(CASObject, CAS, ObjectOS))
    return E;
  if (!ObjectBuffer.equals(InternalBuffer))
    return createStringError(inconvertibleErrorCode(),
                             "CASBackend output round-trip verification error");
  OS << ObjectBuffer;
  return Error::success();
}

uint64_t COFFCASWriter::writeObject() {
  uint64_t StartOffset = OS.tell();

  auto CASObject = CreateFromMCAssembler(*this, *this->Asm, CAS, nullptr);
  if (CASIDOS)
    cas::writeCASIDBuffer(CASObject.getID(), *CASIDOS);
  if (ResultCallBack)
    cantFail((*ResultCallBack)(CASObject.getID()));

  switch (Mode) {
  case CASBackendMode::CASID:
    cas::writeCASIDBuffer(CASObject.getID(), OS);
    break;
  case CASBackendMode::Native:
    if (auto E = SerializeObjectFile(CASObject, CAS, OS))
      report_fatal_error(std::move(E));
    break;
  case CASBackendMode::Verify:
    if (auto E = VerifyObject(CASObject))
      report_fatal_error(std::move(E));
  }

  return OS.tell() - StartOffset;
}

std::unique_ptr<llvm::MCObjectWriter> createCOFFCASWriter(
    std::unique_ptr<MCWinCOFFObjectTargetWriter> W, const Triple &Triple,
    cas::ObjectStore &CAS, CASBackendMode Mode, raw_pwrite_stream &OS,
    std::function<const cas::ObjectProxy(llvm::MCObjectWriter &,
                                         llvm::MCAssembler &,
                                         cas::ObjectStore &, raw_ostream *)>
        CreateFromMCAssembler,
    std::function<Error(cas::ObjectProxy, cas::ObjectStore &, raw_ostream &)>
        SerializeObjectFile,
    std::optional<MCTargetOptions::ResultCallBackTy> ResultCallBack,
    raw_pwrite_stream *CasIDOS) {
  return std::make_unique<COFFCASWriter>(
      std::move(W), Triple, CAS, Mode, OS, CreateFromMCAssembler,
      SerializeObjectFile, ResultCallBack, CasIDOS);
}
} // namespace llvm
