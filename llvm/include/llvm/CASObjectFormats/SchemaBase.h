//===- llvm/CASObjectFormats/SchemaBase.h -----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_SCHEMABASE_H
#define LLVM_CASOBJECTFORMATS_SCHEMABASE_H

#include "llvm/CAS/CASDB.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformats {

/// Schema base class.
class SchemaBase {
  virtual void anchor();

public:
  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  virtual bool isRootNode(const cas::NodeRef &Node) = 0;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  virtual bool isNode(const cas::NodeRef &Node) = 0;

  cas::CASDB &CAS;

  virtual ~SchemaBase() = default;

protected:
  SchemaBase(cas::CASDB &CAS) : CAS(CAS) {}
};

} // namespace casobjectformats
} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_SCHEMABASE_H
