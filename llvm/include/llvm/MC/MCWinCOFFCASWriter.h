//===- llvm/MC/MCWinCOFFCASWriter.h - Mach CAS Object Writer ----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_MCWINCOFFCASWRITER_H
#define LLVM_MC_MCWINCOFFCASWRITER_H

#include "llvm/CAS/ObjectStore.h"
#include "llvm/MC/MCTargetOptions.h"
#include "llvm/MC/MCWinCOFFObjectWriter.h"
#include <functional>
#include <memory>

namespace llvm {

namespace cas {
class ObjectStore;
class CASID;
} // namespace cas

class COFFCASWriter : public WinCOFFObjectWriter {
  const Triple Target;
  cas::ObjectStore &CAS;
  CASBackendMode Mode;
  std::function<const cas::ObjectProxy(MCObjectWriter &, MCAssembler &,
                                       cas::ObjectStore &, raw_ostream *)>
      CreateFromMCAssembler;
  std::function<Error(cas::ObjectProxy, cas::ObjectStore &, raw_ostream &)>
      SerializeObjectFile;
  std::optional<MCTargetOptions::ResultCallBackTy> ResultCallBack;

  raw_pwrite_stream &OS;
  raw_pwrite_stream *CASIDOS;

  // Buffer
  SmallString<512> InternalBuffer;
  raw_svector_ostream InternalOS;

  Error VerifyObject(cas::ObjectProxy CASObject);

public:
  COFFCASWriter(
      std::unique_ptr<MCWinCOFFObjectTargetWriter>, const Triple &Target,
      cas::ObjectStore &CAS, CASBackendMode CBM, raw_pwrite_stream &OS,
      std::function<const cas::ObjectProxy(MCObjectWriter &, MCAssembler &,
                                           cas::ObjectStore &, raw_ostream *)>
          CreateFromMCAssembler,
      std::function<Error(cas::ObjectProxy, cas::ObjectStore &, raw_ostream &)>
          SerializeObjectFile,
      std::optional<MCTargetOptions::ResultCallBackTy> CallBack,
      raw_pwrite_stream *CASIDOS = nullptr);

  uint64_t writeObject() override;
};

/// Construct a new COFF CAS Writer instance.
///
/// This routine takes ownership of the target writer subclass.
///
/// \param TW - The target specific (WinCOFF) writer subclass.
/// \param Target - The target triple.
/// \param CAS - The ObjectStore instance.
/// \param OS - The stream to write to.
/// \returns The constructed object writer.
std::unique_ptr<MCObjectWriter> createCOFFCASWriter(
    std::unique_ptr<MCWinCOFFObjectTargetWriter> TW, const Triple &Triple,
    cas::ObjectStore &CAS, CASBackendMode Mode, raw_pwrite_stream &OS,
    std::function<const cas::ObjectProxy(MCObjectWriter &, MCAssembler &,
                                         cas::ObjectStore &, raw_ostream *)>
        CreateFromMCAssembler,
    std::function<llvm::Error(cas::ObjectProxy, cas::ObjectStore &,
                              raw_ostream &)>
        SerializeObjectFile,
    std::optional<MCTargetOptions::ResultCallBackTy> Callback = std::nullopt,
    raw_pwrite_stream *CasIDOS = nullptr);
} // namespace llvm

#endif
