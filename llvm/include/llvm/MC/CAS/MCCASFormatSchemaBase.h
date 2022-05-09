//===- llvm/MC/CAS/MCCASFormatSchemaBase.h ----------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_MC_CAS_MCCASFORMATSCHEMABASE_H
#define LLVM_MC_CAS_MCCASFORMATSCHEMABASE_H

#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASNodeSchema.h"
#include "llvm/MC/MCAssembler.h"
#include "llvm/MC/MCMachOCASWriter.h"
#include "llvm/Support/Casting.h"
#include "llvm/Support/raw_ostream.h"

namespace llvm {
namespace mccasformats {

class MCFormatSchemaBase
    : public RTTIExtends<MCFormatSchemaBase, cas::NodeSchema> {
  void anchor() override;

public:
  static char ID;

  bool isRootNode(const cas::NodeHandle &Node) const final {
    return isRootNode(cas::NodeProxy::load(CAS, Node));
  }

  bool isNode(const cas::NodeHandle &Node) const final {
    return isNode(cas::NodeProxy::load(CAS, Node));
  }

  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  virtual bool isRootNode(const cas::NodeProxy &Node) const = 0;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  virtual bool isNode(const cas::NodeProxy &Node) const = 0;

  Expected<cas::NodeProxy>
  createFromMCAssembler(llvm::MachOCASWriter &ObjectWriter,
                        llvm::MCAssembler &Asm, const llvm::MCAsmLayout &Layout,
                        raw_ostream *DebugOS = nullptr) const {
    return createFromMCAssemblerImpl(ObjectWriter, Asm, Layout, DebugOS);
  }

  virtual Error serializeObjectFile(cas::NodeProxy RootNode,
                                    llvm::raw_ostream &OS) const = 0;

protected:
  virtual Expected<cas::NodeProxy>
  createFromMCAssemblerImpl(llvm::MachOCASWriter &ObjectWriter,
                            llvm::MCAssembler &Asm,
                            const llvm::MCAsmLayout &Layout,
                            raw_ostream *DebugOS) const = 0;

  MCFormatSchemaBase(cas::CASDB &CAS)
      : MCFormatSchemaBase::RTTIExtends(CAS) {}
};

/// Creates all the schemas and can be used to retrieve a particular schema
/// based on a CAS root node. A client should aim to create and maximize re-use
/// of an instance of this object.
void addMCFormatSchemas(cas::SchemaPool &Pool);

/// Wrapper for a pool that is preloaded with object file schemas.
class MCFormatSchemaPool {
public:
  /// Creates all the schemas up front.
  explicit MCFormatSchemaPool(cas::CASDB &CAS) : Pool(CAS) {
    addMCFormatSchemas(Pool);
  }

  /// Look up the schema for the provided root node. Returns \a nullptr if no
  /// schema was found or it's not actually a root node. The returned \p
  /// ObjectFormatSchemaBase pointer is owned by the \p SchemaPool instance,
  /// therefore it cannot be used beyond the \p SchemaPool instance's lifetime.
  ///
  /// Thread-safe.
  MCFormatSchemaBase *getSchemaForRoot(cas::NodeHandle Node) const {
    return dyn_cast_or_null<MCFormatSchemaBase>(
        Pool.getSchemaForRoot(Node));
  }

  cas::SchemaPool &getPool() { return Pool; }
  cas::CASDB &getCAS() const { return Pool.getCAS(); }

private:
  cas::SchemaPool Pool;
};

} // namespace mccasformat
} // namespace llvm

#endif // LLVM_MC_CAS_MCCASFORMATSCHEMABASE_H
