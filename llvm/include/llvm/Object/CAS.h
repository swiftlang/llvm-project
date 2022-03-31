//===- CAS.h - CAS object file implementation -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the CASObjectFile class, which implement the ObjectFile
// interface for CAS files.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_OBJECT_MACHO_H
#define LLVM_OBJECT_MACHO_H

#include "llvm/CAS/CASID.h"
#include "llvm/CAS/CASDB.h"
#include "llvm/Object/Binary.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Object/SymbolicFile.h"
#include "llvm/Support/Error.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {

namespace object {
class CASObjectFile : public ObjectFile {
public:
  static Expected<std::unique_ptr<CASObjectFile>> create(cas::CASID Object,
                                                         cas::CASDB &DB);

private:
  CASObjectFile(cas::CASID Object, cas::CASDB &DB);

  cas::CASID Object;
  cas::CASDB &CAS;
};

} // namespace object
} // namespace llvm

#endif
