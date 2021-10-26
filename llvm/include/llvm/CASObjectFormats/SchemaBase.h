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

/// Schema base class for object files.
///
/// All calls are expected to be thread-safe.
///
/// Adding a new object file schema currently involves the following:
///
/// 1. Derive from SchemaBase. See \a nestedv1::ObjectFileSchema for an
///    example. Best to add some unit tests.
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
class SchemaBase {
  virtual void anchor();

public:
  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  virtual bool isRootNode(const cas::NodeRef &Node) const = 0;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  virtual bool isNode(const cas::NodeRef &Node) const = 0;

  Expected<cas::NodeRef>
  createFromLinkGraph(const jitlink::LinkGraph &G,
                      raw_ostream *DebugOS = nullptr) const {
    return createFromLinkGraphImpl(G, DebugOS);
  }

  Expected<std::unique_ptr<jitlink::LinkGraph>>
  createLinkGraph(cas::NodeRef RootNode, StringRef Name,
                  jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName,
                  raw_ostream *DebugOS = nullptr) const {
    return createLinkGraphImpl(RootNode, Name, GetEdgeKindName, DebugOS);
  }

  cas::CASDB &CAS;

protected:
  virtual Expected<cas::NodeRef>
  createFromLinkGraphImpl(const jitlink::LinkGraph &G,
                          raw_ostream *DebugOS) const = 0;

  virtual Expected<std::unique_ptr<jitlink::LinkGraph>> createLinkGraphImpl(
      cas::NodeRef RootNode, StringRef Name,
      jitlink::LinkGraph::GetEdgeKindNameFunction GetEdgeKindName,
      raw_ostream *DebugOS) const = 0;

  SchemaBase(cas::CASDB &CAS) : CAS(CAS) {}

public:
  virtual ~SchemaBase() = default;
};

} // namespace casobjectformats
} // namespace llvm

#endif // LLVM_CASOBJECTFORMATS_SCHEMABASE_H
