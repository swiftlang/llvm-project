//===- llvm/CASObjectFormats/ObjectFormatSchemaBase.h -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CASOBJECTFORMATS_OBJECTFORMATSCHEMABASE_H
#define LLVM_CASOBJECTFORMATS_OBJECTFORMATSCHEMABASE_H

#include "llvm/CAS/CASDB.h"
#include "llvm/CAS/CASNodeSchema.h"
#include "llvm/CASObjectFormats/Data.h"
#include "llvm/ExecutionEngine/JITLink/JITLink.h"

namespace llvm {
namespace casobjectformats {

namespace reader {
class CASObjectReader;
}

/// Schema base class for object files.
///
/// All calls are expected to be thread-safe.
///
/// Adding a new object file schema currently involves the following:
///
/// 1. Derive from ObjectFormatSchemaBase. See \a nestedv1::ObjectFormatSchema
/// for
///    an example. Best to add some unit tests.
/// 2. Add ingestion support in llvm-cas-object-format.cpp by updating its
///    factory method for creating a schema.
/// 3. Optional: Add stats support in llvm-cas-object-format.cpp by adding the
///    schema and a node handler to StatCollector::Schemas.
///
/// TODO: Extract the non-object-file bits somewhere else. This derived class
/// would add LinkGraph APIs.
///
/// TODO: Maybe allow schemas to be registered somewhere? Maybe just
/// object-file schemas?
class ObjectFormatSchemaBase
    : public RTTIExtends<ObjectFormatSchemaBase, cas::NodeSchema> {
  void anchor() override;

public:
  static char ID;

  virtual Expected<std::unique_ptr<reader::CASObjectReader>>
  createObjectReader(cas::ObjectProxy RootNode) const = 0;

  Expected<cas::ObjectProxy>
  createFromLinkGraph(const jitlink::LinkGraph &G,
                      raw_ostream *DebugOS = nullptr) const {
    return createFromLinkGraphImpl(G, DebugOS);
  }

protected:
  virtual Expected<cas::ObjectProxy>
  createFromLinkGraphImpl(const jitlink::LinkGraph &G,
                          raw_ostream *DebugOS) const = 0;

  ObjectFormatSchemaBase(cas::CASDB &CAS)
      : ObjectFormatSchemaBase::RTTIExtends(CAS) {}
};

/// Creates all the schemas and can be used to retrieve a particular schema
/// based on a CAS root node. A client should aim to create and maximize re-use
/// of an instance of this object.
void addObjectFormatSchemas(cas::SchemaPool &Pool);

Expected<std::unique_ptr<reader::CASObjectReader>>
createObjectReader(const cas::SchemaPool &Pool, cas::CASID ID);

/// Wrapper for a pool that is preloaded with object file schemas.
class ObjectFormatSchemaPool {
public:
  /// Creates all the schemas up front.
  explicit ObjectFormatSchemaPool(cas::CASDB &CAS) : Pool(CAS) {
    addObjectFormatSchemas(Pool);
  }

  /// Look up the schema for the provided root node. Returns \a nullptr if no
  /// schema was found or it's not actually a root node. The returned \p
  /// ObjectFormatSchemaBase pointer is owned by the \p SchemaPool instance,
  /// therefore it cannot be used beyond the \p SchemaPool instance's lifetime.
  ///
  /// Thread-safe.
  ObjectFormatSchemaBase *getSchemaForRoot(cas::ObjectProxy Node) const {
    return dyn_cast_or_null<ObjectFormatSchemaBase>(
        Pool.getSchemaForRoot(Node));
  }

  /// Convenience function that wraps the free function \a
  /// casobjectformats::createObjectReader().
  Expected<std::unique_ptr<reader::CASObjectReader>>
  createObjectReader(cas::CASID ID) const;

  cas::SchemaPool &getPool() { return Pool; }
  cas::CASDB &getCAS() const { return Pool.getCAS(); }

private:
  cas::SchemaPool Pool;
};

} // namespace casobjectformats
} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_OBJECTFORMATSCHEMABASE_H
