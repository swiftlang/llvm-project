//===- llvm/BitstreamCAS/BitstreamCASObjectV1.h ------------------------------*-
// C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_BITSTREAM_CAS_BITSTREAMCASOBJECT_H
#define LLVM_BITSTREAM_CAS_BITSTREAMCASOBJECT_H

#include "llvm/CAS/CASID.h"
#include "llvm/CAS/ObjectStore.h"

namespace llvm {

namespace bitstreamcasformats {
namespace v1 {

class BitstreamSchema;
class BitstreamCASBuilder;
class BitstreamCASReader;

class BitstreamObjectProxy : public cas::ObjectProxy {
public:
  static Expected<BitstreamObjectProxy> get(const BitstreamSchema &Schema,
                                            Expected<cas::ObjectProxy> Ref);
  StringRef getKindString() const;

  /// Return the data skipping the type-id character.
  StringRef getData() const { return cas::ObjectProxy::getData().drop_front(); }

  const BitstreamSchema &getSchema() const { return *Schema; }

  bool operator==(const BitstreamObjectProxy &RHS) const {
    return Schema == RHS.Schema && cas::CASID(*this) == cas::CASID(RHS);
  }

  BitstreamObjectProxy() = delete;

  static Error encodeReferences(ArrayRef<cas::ObjectRef> Refs,
                                SmallVectorImpl<char> &Data,
                                SmallVectorImpl<cas::ObjectRef> &IDs);

  static Expected<SmallVector<cas::ObjectRef>>
  decodeReferences(const BitstreamObjectProxy &Node, StringRef &Remaining);

protected:
  BitstreamObjectProxy(const BitstreamSchema &Schema,
                       const cas::ObjectProxy &Node)
      : cas::ObjectProxy(Node), Schema(&Schema) {}

  class Builder {
  public:
    static Expected<Builder> startRootNode(const BitstreamSchema &Schema,
                                           StringRef KindString);
    static Expected<Builder> startNode(const BitstreamSchema &Schema,
                                       StringRef KindString);

    Expected<BitstreamObjectProxy> build();

  private:
    Error startNodeImpl(StringRef KindString);

    Builder(const BitstreamSchema &Schema) : Schema(&Schema) {}
    const BitstreamSchema *Schema;

  public:
    SmallString<256> Data;
    SmallVector<cas::ObjectRef, 16> Refs;
  };

private:
  const BitstreamSchema *Schema;
};

/// Schema for a DAG in a CAS.
class BitstreamSchema final
    : public RTTIExtends<BitstreamSchema, BitstreamFormatSchemaBase> {
  void anchor() override;

public:
  static char ID;
  std::optional<StringRef> getKindString(const cas::ObjectProxy &Node) const;
  std::optional<unsigned char> getKindStringID(StringRef KindString) const;

  cas::ObjectRef getRootNodeTypeID() const { return *RootNodeTypeID; }

  /// Check if \a Node is a root (entry node) for the schema. This is a strong
  /// check, since it requires that the first reference matches a complete
  /// type-id DAG.
  bool isRootNode(const cas::ObjectProxy &Node) const override;

  /// Check if \a Node could be a node in the schema. This is a weak check,
  /// since it only looks up the KindString associated with the first
  /// character. The caller should ensure that the parent node is in the schema
  /// before calling this.
  bool isNode(const cas::ObjectProxy &Node) const override;

  // We will eventually define the method in .cpp file.
  Error serializeBitstreamFile(cas::ObjectProxy RootNode,
                               raw_ostream &OS) const override {
    if (!isRootNode(RootNode))
      return createStringError(inconvertibleErrorCode(), "invalid root node");
    auto Bitstream = BitstreamRef::get(*this, RootNode.getRef());
    if (!Bitstream)
      return Bitstream.takeError();
    return Bitstream->materialize(OS);
  }

  BitstreamSchema(cas::ObjectStore &CAS);

  Expected<BitstreamObjectProxy>
  create(ArrayRef<cas::ObjectRef> Refs, // CHECK THIS.
         StringRef Data) const {
    return BitstreamObjectProxy::get(*this, CAS.createProxy(Refs, Data));
  }
  Expected<BitstreamObjectProxy> get(cas::ObjectRef ID) const {
    return BitstreamObjectProxy::get(*this, CAS.getProxy(ID));
  }
};

/// A type-checked reference to a node of a specific kind.
template <class DerivedT, class FinalT = DerivedT>
class SpecificRef : public BitstreamObjectProxy {
protected:
  static Expected<DerivedT> get(Expected<BitstreamObjectProxy> Ref) {
    if (auto Specific = getSpecific(std::move(Ref)))
      return DerivedT(*Specific);
    else
      return Specific.takeError();
  }

  static Expected<SpecificRef> getSpecific(Expected<BitstreamObjectProxy> Ref) {
    if (!Ref)
      return Ref.takeError();
    if (Ref->getKindString() == FinalT::KindString)
      return SpecificRef(*Ref);
    return createStringError(inconvertibleErrorCode(),
                             "expected Bitstream object '" +
                                 FinalT::KindString + "'");
  }

  static std::optional<SpecificRef> Cast(BitstreamObjectProxy Ref) {
    if (Ref.getKindString() == FinalT::KindString)
      return SpecificRef(Ref);
    return std::nullopt;
  }

  SpecificRef(BitstreamObjectProxy Ref) : BitstreamObjectProxy(Ref) {}
};

// Root node:
class BitstreamRef : public SpecificRef<BitstreamRef> {
public:
  // This and the subsquent Ref's create method will eventually take
  // BitstreamCASBuilder as an argument.
  static Expected<BitstreamRef> create(SmallVector<cas::ObjectRef> &refs,
                                       SmallString &data) {}

  // This uses the BitstreamCASReader to materialize the BitstreamRef and calls
  // the child nodes recursively to materialize the entire DAG.
  Error materialize(raw_ostream &OS) const;
};

// Block info block related:
class BlockInfoBlockRef : public SpecificRef<BlockInfoBlockRef> {
public:
  static Expected<BlockInfoBlockRef> create(SmallVector<cas::ObjectRef> &refs,
                                            SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class SetBidRecordRef : public SpecificRef<SetBidRecordRef> {
public:
  static Expected<SetBidRecordRef> create(SmallVector<cas::ObjectRef> &refs,
                                          SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class BlockNameRecordRef : public SpecificRef<BlockNameRecordRef> {
public:
  static Expected<BlockNameRecordRef> create(SmallVector<cas::ObjectRef> &refs,
                                             SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class DefineAbbrevRecordRef : public SpecificRef<DefineAbbrevRecordRef> {
public:
  static Expected<DefineAbbrevRecordRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class SetRecordNameRecordRef : public SpecificRef<SetRecordNameRecordRef> {
public:
  static Expected<SetRecordNameRecordRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

// Common block related:
class GenericBlockRef : public SpecificRef<GenericBlockRef> {
public:
  static Expected<GenericBlockRef> create(SmallVector<cas::ObjectRef> &refs,
                                          SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class GenericRecordRef : public SpecificRef<GenericRecordRef> {
public:
  static Expected<GenericRecordRef> create(SmallVector<cas::ObjectRef> &refs,
                                           SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class GenericRecordWithAbbrevRef
    : public SpecificRef<GenericRecordWithAbbrevRef> {
public:
  static Expected<GenericRecordWithAbbrevRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class GenericRecordWithBlobRef : public SpecificRef<GenericRecordWithBlobRef> {
public:
  static Expected<GenericRecordWithBlobRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class GenericRecordWithArrayRef
    : public SpecificRef<GenericRecordWithArrayRef> {
public:
  static Expected<GenericRecordWithArrayRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class DefineAbbrevRecordRef : public SpecificRef<DefineAbbrevRecordRef> {
public:
  static Expected<DefineAbbrevRecordRef>
  create(SmallVector<cas::ObjectRef> &refs, SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class UnAbbrevRecordRef : public SpecificRef<UnAbbrevRecordRef> {
public:
  static Expected<UnAbbrevRecordRef> create(SmallVector<cas::ObjectRef> &refs,
                                            SmallString &data);

  Expected<uint64_t> materialize(BitstreamCASReader &Reader,
                                 raw_ostream *Stream);
};

class BitstreamCASReader {
public:
  raw_ostream &OS;

  BitstreamCASReader(raw_ostream &OS, const Triple &Target,
                     const BitstreamSchema &Schema);
  endianness getEndian() {
    return Target.isLittleEndian() ? endianness::little : endianness::big;
  }

  Expected<BitstreamObjectProxy> getObjectProxy(cas::ObjectRef ID) {
    auto Node = BitstreamObjectProxy::get(Schema, Schema.CAS.getProxy(ID));
    if (!Node)
      return Node.takeError();
    return Node;
  }

  Triple::ArchType getArch() { return Target.getArch(); }

  Expected<uint64_t> materializeHeaderInfo(cas::ObjectRef ID);
  Expected<uint64_t> materializeRecord(cas::ObjectRef ID);
  Expected<uint64_t> materializeBlock(cas::ObjectRef ID);

private:
  const Triple &Target;
  const BitstreamSchema &Schema;
};

class BitstreamCASBuilder {};

} // namespace v1
} // namespace bitstreamcasformats
} // namespace llvm

#endif // LLVM_BITSTREAM_CAS_BITSTREAMCASOBJECT_H
