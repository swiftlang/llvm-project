//===--- Pointer.h - Types for the constexpr VM -----------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the classes responsible for pointer tracking.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_POINTER_H
#define LLVM_CLANG_AST_INTERP_POINTER_H

#include "CmpResult.h"
#include "Descriptor.h"
#include "InterpBlock.h"
#include "clang/AST/ComparisonCategories.h"
#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "clang/AST/Expr.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/Support/raw_ostream.h"

namespace clang {
namespace interp {
class Block;
class DeadBlock;
class Context;
class InterpState;
class Pointer;
class Function;
enum PrimType : unsigned;

/// Enumeration of pointer implementation kinds.
enum PointerKind {
  /// Null pointer.
  TargetPtr,
  /// Pointer to a heap block.
  BlockPtr,
  /// Pointer to unreadable data.
  ExternPtr,
  /// Type info pointer.
  TypeInfoPtr,
  /// Invalid pointer.
  InvalidPtr,
};

/// Pointer to VM data.
class BlockPointer {
public:
  /// Creates a copy of a block pointer.
  BlockPointer(const BlockPointer &I)
      : K(PointerKind::BlockPtr), Pointee(I.Pointee), Base(I.Base),
        Offset(I.Offset) {}

  /// Overwrites an object with another.
  BlockPointer &operator=(const BlockPointer &P) {
    Pointee = P.Pointee;
    Base = P.Base;
    Offset = P.Offset;
    return *this;
  }

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Offsets a pointer inside an array.
  BlockPointer atIndex(uint64_t Idx) const;
  /// Creates a pointer to a field.
  BlockPointer atField(unsigned Off) const;
  /// Restricts the scope of an array element pointer.
  BlockPointer decay() const;
  /// Returns a pointer to the object of which this pointer is a field.
  BlockPointer getBase() const;
  /// Returns the parent array.
  BlockPointer getArray() const;
  /// Creates a pointer to a base class.
  BlockPointer atBase(unsigned Off, bool IsVirtual) const {
    return atField(Off);
  }

  /// Checks if the pointer is live.
  bool isLive() const;
  /// Checks if the innermost field is an array.
  bool inArray() const { return getArrayDesc()->IsArray; }
  /// Checks if the structure is an array of unknown size.
  bool isUnknownSize() const { return getArrayDesc()->isUnknownSize(); }
  /// Pointer points directly to a block.
  bool isRoot() const;
  /// Checks if the storage is static.
  bool isStatic() const { return Pointee->isStatic(); }
  /// Checks if the storage is temporary.
  bool isTemporary() const { return Pointee->isTemporary(); }
  /// Checks if the field is mutable.
  bool isMutable() const;
  /// Checks if an object was initialized.
  bool isInitialized() const;
  /// Checks if the object is active.
  bool isActive() const;
  /// Checks if the pointer is to a base class.
  bool isBaseClass() const;
  /// Checks if an object or a subfield is mutable.
  bool isConst() const;
  /// Checks if the index is one past end.
  bool isOnePastEnd() const { return getSize() == getOffset(); }
  /// Checks if the storage is a static temporary.
  bool isStaticTemporary() const { return isStatic() && isTemporary(); }
  /// Checks if the pointer is a literal.
  bool isLiteral() const;

  /// Block pointers are never null.
  bool isZero() const { return false; }
  bool isNonZero() const { return true; }

  /// Accessors for information about the declaration site.
  const ValueDecl *getDecl() const { return getDeclDesc()->asValueDecl(); }
  /// Returns the type of the declaration site.
  QualType getDeclType() const { return getDeclDesc()->getType(); }
  /// Returns the declaration if pointers points to a field.
  const FieldDecl *getField() const { return getFieldDesc()->asFieldDecl(); }
  /// Returns the type of the innermost field.
  QualType getFieldType() const;
  /// Returns the declaration location.
  SourceLocation getDeclLoc() const { return getDeclDesc()->getLocation(); }
  /// Returns the declaration ID.
  llvm::Optional<unsigned> getDeclID() const { return Pointee->getDeclID(); }
  /// Returns the record descriptor of a class.
  Record *getRecord() const { return getFieldDesc()->ElemRecord; }
  /// Returns the primitive type of array elements.
  llvm::Optional<PrimType> elementType() const;

  /// Returns the element size of the innermost field.
  size_t elemSize() const { return getArrayDesc()->getElemSize(); }
  /// Returns the total size of the innermost field.
  size_t getSize() const { return getArrayDesc()->getSize(); }
  /// Returns the offset into the innermost field.
  unsigned getOffset() const;
  /// Returns the index into an array.
  uint64_t getIndex() const;
  /// Returns the number of elements.
  uint64_t getNumElems() const { return getSize() / elemSize(); }

  /// Checks if two pointers point to a common object.
  bool hasSameBase(const BlockPointer &RHS) const;
  /// Checks if two pointers can be subtracted.
  bool hasSameArray(const BlockPointer &RHS) const;
  /// Compares two pointers from the same block.
  CmpResult compare(const BlockPointer &RHS) const;
  /// Prints the block pointer to a stream.
  void print(llvm::raw_ostream &OS) const;
  /// Prints the block pointer to a stream for diagnostics.
  void print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const;
  /// Converts the pointer to a string.
  std::string getAsString(ASTContext &Ctx, QualType Ty) const;
  /// Compares two pointers to blocks.
  bool operator==(const BlockPointer &RHS) const;

  /// Initialises the field to which the pointer points.
  void initialize() const;
  /// Activates the field to which the pointer points.
  void activate() const;
  /// Deactivates the field to which the pointer points.
  void deactivate() const;

  /// Dereferences the pointer, if it's live.
  template <typename T> T &deref() const {
    return *reinterpret_cast<T *>(Pointee->data() + Offset);
  }

  /// Dereferences a primitive element.
  template <typename T> T &elem(unsigned I) const {
    return reinterpret_cast<T *>(Pointee->data())[I];
  }

private:
  BlockPointer(Block *Pointee) : K(PointerKind::BlockPtr), Pointee(Pointee) {}

  BlockPointer(Block *Pointee, unsigned Base, unsigned Offset)
      : K(PointerKind::BlockPtr), Pointee(Pointee), Base(Base), Offset(Offset) {
    assert(Base % alignof(void *) == 0 && "base");
  }

  BlockPointer getContainingObject() const {
    return isArrayElement() ? getArray() : getBase();
  }

  /// Checks if the item is a field in an object.
  bool isField() const;
  /// Checks if the pointer is to an array element.
  bool isArrayElement() const;

  Descriptor *getDeclDesc() const;
  Descriptor *getFieldDesc() const;
  Descriptor *getArrayDesc() const;

  /// Returns a descriptor at a given offset.
  InlineDescriptor *getDescriptor(unsigned Offset) const {
    assert(Offset != 0 && "Not a nested pointer");
    return reinterpret_cast<InlineDescriptor *>(Pointee->data() + Offset) - 1;
  }

  /// Returns a reference to the pointer which stores the initialization map.
  InitMap *&getInitMap() const {
    return *reinterpret_cast<InitMap **>(Pointee->data() + Base);
  }

private:
  friend class Pointer;
  friend class Block;
  friend class DeadBlock;

  /// Pointer kind - first field.
  PointerKind K;
  /// The block the pointer is pointing to.
  Block *Pointee = nullptr;
  /// Start of the current subfield.
  unsigned Base = 0;
  /// Offset into the block.
  unsigned Offset = 0;
  /// Previous link in the pointer chain.
  BlockPointer *Prev = nullptr;
  /// Next link in the pointer chain.
  BlockPointer *Next = nullptr;
};

/// Pointer to dummy data.
///
/// External pointers can be derived in constexpr, however
/// the data to which they point cannot be accessed.
class ExternPointer {
private:
  /// A path into an allocation.
  struct PointerPath {
    /// Path of nested accesses.
    struct Entry {
      /// Enumeration of subfield kinds.
      enum class Kind {
        /// Base class.
        Base,
        /// Virtual base class.
        Virt,
        /// Member of a record.
        Field,
        /// Element of an array.
        Index,
      } K;

      /// Descriptor of the subfield.
      Descriptor *Desc;
      /// Subfield identifier.
      union Field {
        /// Identifier of the base class.
        const CXXRecordDecl *BaseEntry;
        /// Identifier of the field.
        const FieldDecl *FieldEntry;
        /// Index of an array element.
        uint64_t Index;
      } U;

      /// Checks if an entry is for an index.
      bool isBase() const { return K == Kind::Base; }
      bool isField() const { return K == Kind::Field; }
      bool isIndex() const { return K == Kind::Index; }

      Entry(Descriptor *Desc, const CXXRecordDecl *Base, bool IsVirtual)
          : K(IsVirtual ? Kind::Virt : Kind::Base), Desc(Desc) {
        U.BaseEntry = Base;
      }

      Entry(Descriptor *Desc, const FieldDecl *Field)
          : K(Kind::Field), Desc(Desc) {
        U.FieldEntry = Field;
      }

      Entry(Descriptor *Desc, uint64_t Index) : K(Kind::Index), Desc(Desc) {
        U.Index = Index;
      }

      bool operator==(const Entry &RHS) {
        if (K != RHS.K)
          return false;
        switch (K) {
        case Kind::Base:
          return U.BaseEntry == RHS.U.BaseEntry;
        case Kind::Virt:
          return U.BaseEntry == RHS.U.BaseEntry;
        case Kind::Field:
          return U.FieldEntry == RHS.U.FieldEntry;
        case Kind::Index:
          return U.Index == RHS.U.Index;
        }
      }

      bool operator!=(const Entry &RHS) { return !(*this == RHS); }
    };

    /// List of entries.
    llvm::SmallVector<Entry, 4> Entries;

    /// Constructs a path to a root declaration.
    PointerPath() {}
    /// Constructs a path from a list of entries.
    PointerPath(ArrayRef<Entry> Entries)
        : Entries(Entries.begin(), Entries.end()) {}
  };
public:
  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Offsets a pointer inside an array.
  ExternPointer atIndex(uint64_t Idx) const;
  /// Creates a pointer to a field.
  ExternPointer atField(unsigned Off) const;
  /// Restricts the scope of an array element pointer.
  ExternPointer decay() const;
  /// Returns a pointer to the object of which this pointer is a field.
  ExternPointer getBase() const;
  /// Creates a pointer to a base class.
  ExternPointer atBase(unsigned Off, bool IsVirtual) const;

  /// Checks if the index is one past end.
  bool isOnePastEnd() const;
  /// Checks if the innermost field is an array.
  bool inArray() const;
  /// Checks if the structure is an array of unknown size.
  bool isUnknownSize() const { return getArrayDesc()->isUnknownSize(); }
  /// Checks if the pointer is to a base class.
  bool isBaseClass() const;
  /// Checks if the pointer is a literal.
  bool isLiteral() const { return false; }

  /// Extern pointers can be both null or non-null when modelling a parameter.
  bool isZero() const { return false; }
  bool isNonZero() const { return !isa<ParmVarDecl>(Base); }

  /// Accessors for information about the declaration site.
  const ValueDecl *getDecl() const { return Base; }
  /// Returns the type of the declaration site.
  QualType getDeclType() const { return Base->getType(); }
  /// Returns the declaration if pointers points to a field.
  const FieldDecl *getField() const { return getFieldDesc()->asFieldDecl(); }
  /// Returns the type of the innermost field.
  QualType getFieldType() const { return getFieldDesc()->getType(); }
  /// Returns the declaration location.
  SourceLocation getDeclLoc() const { return Base->getLocation(); }
  /// Returns the record descriptor of a class.
  Record *getRecord() const { return getFieldDesc()->ElemRecord; }

  /// Returns the index into an array.
  uint64_t getIndex() const;
  /// Returns the number of elements.
  uint64_t getNumElems() const { return getArrayDesc()->getNumElems(); }

  /// Checks if two pointers point to a common object.
  bool hasSameBase(const ExternPointer &RHS) const;
  /// Checks if two pointers can be subtracted.
  bool hasSameArray(const ExternPointer &RHS) const;
  /// Compares two pointers from the same block.
  CmpResult compare(const ExternPointer &RHS) const;
  /// Prints the block pointer to a stream.
  void print(llvm::raw_ostream &OS) const;
  /// Prints the block pointer to a stream for diagnostics.
  void print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const;
  /// Converts the pointer to a string.
  std::string getAsString(ASTContext &Ctx, QualType Ty) const;
  /// Compares two pointers to the same external object.
  bool operator==(const ExternPointer &RHS) const;

private:
  ExternPointer(const ExternPointer &D)
      : K(PointerKind::ExternPtr), Desc(D.Desc), Base(D.Base),
        Path(std::make_unique<PointerPath>(*D.Path)) {}

  ExternPointer(ExternPointer &&D)
      : K(PointerKind::ExternPtr), Desc(D.Desc), Base(D.Base),
        Path(std::move(D.Path)) {}

  ExternPointer(Descriptor *Desc, const ValueDecl *Base)
      : K(PointerKind::ExternPtr), Desc(Desc), Base(Base),
        Path(std::make_unique<PointerPath>()) {}

  ExternPointer(const ExternPointer &P, PointerPath &&Path)
      : K(PointerKind::ExternPtr), Desc(P.Desc), Base(P.Base),
        Path(std::make_unique<PointerPath>(std::move(Path))) {}

  Descriptor *getFieldDesc() const;
  Descriptor *getArrayDesc() const;

private:
  friend class Pointer;

  /// Pointer kind - first field.
  PointerKind K;
  /// Declaration descriptor.
  Descriptor *Desc;
  /// Declaration identifier.
  const ValueDecl *Base;
  /// The actual data is heap allocated since it would
  /// not fit in the size of regular block pointers.
  std::unique_ptr<PointerPath> Path;
};

/// A pointer derived from null or a fixed address.
class TargetPointer {
public:
  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Offsets a pointer inside an array.
  TargetPointer atIndex(uint64_t Idx) const;
  /// Creates a pointer to a field.
  TargetPointer atField(unsigned Off) const;
  /// Restricts the scope of an array element pointer.
  TargetPointer decay() const;
  /// Returns a pointer to the object of which this pointer is a field.
  TargetPointer getBase() const;
  /// Creates a pointer to a base class.
  TargetPointer atBase(unsigned Off, bool IsVirtual) const;

  /// Checks if the index is one past end.
  bool isOnePastEnd() const { return Desc && getIndex() == getNumElems(); }
  /// Checks if the innermost field is an array.
  bool inArray() const { return InArray; }
  /// Checks if the structure is an array of unknown size.
  bool isUnknownSize() const { return Desc && getArrayDesc()->isUnknownSize(); }
  /// Checks if the pointer is to a base class.
  bool isBaseClass() const;
  /// Checks if the pointer is null.
  bool isZero() const;
  /// Checks if the pointer is non-null.
  bool isNonZero() const { return !isZero(); }
  /// Checks if the pointer is a literal.
  bool isLiteral() const { return false; }

  /// Returns the declaration if pointers points to a field.
  const FieldDecl *getField() const { return getFieldDesc()->asFieldDecl(); }
  /// Returns the type of the innermost field.
  QualType getFieldType() const { return getFieldDesc()->getType(); }
  /// Returns the record descriptor of a class.
  Record *getRecord() const { return getFieldDesc()->ElemRecord; }

  /// Returns the index into an array.
  uint64_t getIndex() const;
  /// Returns the number of elements.
  uint64_t getNumElems() const;

  /// Checks if two pointers can be subtracted.
  bool hasSameArray(const TargetPointer &RHS) const;
  /// Compares two pointers from the same block.
  CmpResult compare(const TargetPointer &RHS) const;
  /// Prints the block pointer to a stream.
  void print(llvm::raw_ostream &OS) const;
  /// Prints the block pointer to a stream for diagnostics.
  void print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const;
  /// Converts the pointer to a string.
  std::string getAsString(ASTContext &Ctx, QualType Ty) const;
  /// Compares two pointers to the same external object.
  bool operator==(const TargetPointer &RHS) const;

  /// Returns the offset of fields/elements relative to the pointer.
  CharUnits getTargetOffset() const {
    return BaseOffset + FieldOffset + ArrayOffset;
  }

private:
  TargetPointer() : K(PointerKind::TargetPtr) {}

  TargetPointer(Descriptor *Desc) : K(PointerKind::TargetPtr), Desc(Desc) {}

  TargetPointer(Descriptor *Desc, CharUnits Base)
      : K(PointerKind::TargetPtr), Desc(Desc), BaseOffset(Base) {}

  TargetPointer(Descriptor *Desc, CharUnits BaseOffset, CharUnits FieldOffset,
                CharUnits ArrayOffset, bool InArray)
      : K(PointerKind::TargetPtr), Desc(Desc), BaseOffset(BaseOffset),
        FieldOffset(FieldOffset), ArrayOffset(ArrayOffset), InArray(InArray) {}

  Descriptor *getFieldDesc() const {
    return InArray ? Desc->getElemDesc() : Desc;
  }
  Descriptor *getArrayDesc() const { return Desc; }

private:
  friend class Pointer;

  /// Pointer kind - first field.
  PointerKind K;
  /// Descriptor of the current field.
  Descriptor *Desc = nullptr;
  /// Base address.
  CharUnits BaseOffset = CharUnits::Zero();
  /// Offset to the a field of the pointer.
  CharUnits FieldOffset = CharUnits::Zero();
  /// Offset into an array type.
  CharUnits ArrayOffset = CharUnits::Zero();
  /// Flag to distinguish arrays from array elements.
  bool InArray = false;
};

/// A pointer to a typeid block.
///
/// Carries std::type_info information.
class TypeInfoPointer {
public:
  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Offsets a pointer inside an array.
  TypeInfoPointer atIndex(uint64_t Idx) const;
  /// Creates a pointer to a field.
  TypeInfoPointer atField(unsigned Off) const;
  /// Restricts the scope of an array element pointer.
  TypeInfoPointer decay() const;
  /// Returns a pointer to the object of which this pointer is a field.
  TypeInfoPointer getBase() const;
  /// Creates a pointer to a base class.
  TypeInfoPointer atBase(unsigned Off, bool IsVirtual) const;

  /// Checks if the index is one past end.
  bool isOnePastEnd() const { return getIndex() == getNumElems(); }
  /// Checks if the innermost field is an array.
  bool inArray() const { return false; }
  /// Checks if the structure is an array of unknown size.
  bool isUnknownSize() const { return false; }
  /// Checks if the pointer is to a base class.
  bool isBaseClass() const { return true; }
  /// Checks if the pointer is null.
  bool isZero() const { return false; }
  /// Checks if the pointer is non-null.
  bool isNonZero() const { return true; }
  /// Checks if the pointer is a literal.
  bool isLiteral() const { return false; }

  /// Returns the declaration if pointers points to a field.
  const FieldDecl *getField() const;
  /// Returns the type of the innermost field.
  QualType getFieldType() const;
  /// Returns the record descriptor of a class.
  Record *getRecord() const;

  /// Returns the index into an array.
  uint64_t getIndex() const { return Index; }
  /// Returns the number of elements.
  uint64_t getNumElems() const { return 1; }

  /// Checks if two pointers point to a common object.
  bool hasSameBase(const TypeInfoPointer &RHS) const { return Ty == RHS.Ty; }
  /// Checks if two pointers can be subtracted.
  bool hasSameArray(const TypeInfoPointer &RHS) const { return Ty == RHS.Ty; }
  /// Compares two pointers from the same block.
  CmpResult compare(const TypeInfoPointer &RHS) const;
  /// Prints the block pointer to a stream.
  void print(llvm::raw_ostream &OS) const;
  /// Prints the block pointer to a stream for diagnostics.
  void print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const;
  /// Converts the pointer to a string.
  std::string getAsString(ASTContext &Ctx, QualType Ty) const;
  /// Compares two pointers to the same external object.
  bool operator==(const TypeInfoPointer &RHS) const;

private:
  TypeInfoPointer(const Type *Ty, QualType InfoTy, unsigned Index = 0)
      : K(PointerKind::TypeInfoPtr), Ty(Ty), InfoTy(InfoTy), Index(Index) {}

private:
  friend class Pointer;

  /// Pointer kind - first field.
  PointerKind K;
  /// Underlying type.
  const Type *Ty;
  /// Type of the typeinfo structure.
  QualType InfoTy;
  /// Flag to indicate if the pointer is one-past-end.
  unsigned Index;
};


/// A pointer to a memory block, live or dead.
///
/// This object can be allocated into interpreter stack frames. If pointing to
/// a live block, it is a link in the chain of pointers pointing to the block.
class Pointer {
public:
  struct Invalid {};

  /// Creates a null pointer.
  Pointer() : U(static_cast<Descriptor *>(nullptr)) {}
  /// Creates a null pointer cast to a specific type.
  Pointer(Descriptor *Desc) : U(Desc) {}
  /// Creates a pointer to a fixed target address.
  Pointer(Descriptor *Desc, CharUnits Base) : U(Desc, Base) {}
  /// Creates a block pointer.
  Pointer(Block *B) : U(B) {}
  /// Creates an external pointer.
  Pointer(Descriptor *Desc, const ValueDecl *VD) : U(Desc, VD) {}
  /// Creates an invalid pointer.
  Pointer(Invalid) : U() {}
  /// Creates a pointer to type information.
  Pointer(const Type *Ty, QualType InfoTy) : U(Ty, InfoTy) {}

  /// Creates a pointer from a block pointer.
  Pointer(BlockPointer &&Ptr) : U(std::move(Ptr)) {}
  /// Creates a pointer from an external pointer.
  Pointer(ExternPointer &&Ptr) : U(std::move(Ptr)) {}
  /// Creates a pointer from a null pointer.
  Pointer(TargetPointer &&Ptr) : U(std::move(Ptr)) {}
  /// Creates a pointer from a typeinfo pointer.
  Pointer(TypeInfoPointer &&Ptr) : U(std::move(Ptr)) {}

  /// Copies a pointer.
  Pointer(const Pointer &P) : U(P.U) {}
  /// Moves a pointer, managing chains more efficiently.
  Pointer(Pointer &&P) : U(std::move(P.U)) {}

  /// Converts the pointer to an APValue.
  bool toAPValue(const ASTContext &Ctx, APValue &Result) const;

  /// Offsets a pointer inside an array.
  Pointer atIndex(uint64_t Idx) const;
  /// Creates a pointer to a field.
  Pointer atField(unsigned Off) const;
  /// Restricts the scope of an array element pointer.
  Pointer decay() const;
  /// Returns a pointer to the object of which this pointer is a field.
  Pointer getBase() const;
  /// Creates a pointer to a base class.
  Pointer atBase(unsigned Off, bool IsVirtual) const;

  /// Checks if an object was initialized.
  bool isInitialized() const;
  /// Checks if the innermost field is an array.
  bool inArray() const;
  /// Checks if the structure is an array of unknown size.
  bool isUnknownSize() const;
  /// Checks if the pointer is to a base class.
  bool isBaseClass() const;
  /// Checks if the index is one past end.
  bool isOnePastEnd() const;
  /// Checks if the pointer is always null.
  bool isZero() const;
  /// Checks if the pointer can never be null.
  bool isNonZero() const;
  /// Checks if the pointer is weak.
  bool isWeak() const;
  /// Checks if the pointer is to a literal.
  bool isLiteral() const;
  /// Checks if the pointer is convertible to bool.
  bool convertsToBool() const;

  /// Returns the declaration if pointers points to a field.
  const FieldDecl *getField() const;
  /// Returns the type of the innermost field.
  QualType getFieldType() const;
  /// Returns the record descriptor of a class.
  Record *getRecord() const;

  /// Returns the index into an array.
  uint64_t getIndex() const;
  /// Returns the number of elements.
  unsigned getNumElems() const;

  /// Checks if two pointers are comparable.
  bool hasSameBase(const Pointer &RHS) const;
  /// Checks if two pointers can be subtracted.
  bool hasSameArray(const Pointer &RHS) const;
  /// Compares two pointers from the same block.
  CmpResult compare(const Pointer &RHS) const;
  /// Prints the block pointer to a stream.
  void print(llvm::raw_ostream &OS) const;
  /// Prints the block pointer to a stream for diagnostics.
  void print(llvm::raw_ostream &OS, ASTContext &Ctx, QualType Ty) const;
  /// Converts the pointer to a string.
  std::string getAsString(ASTContext &Ctx, QualType Ty) const;
  /// Checks if two pointers are equal.
  bool operator==(const Pointer &RHS) const;
  /// Copies a pointer.
  void operator=(const Pointer &P);
  /// Moves a pointer into this.
  void operator=(Pointer &&P);

  /// Tests if the pointer is valid.
  bool isValid() { return U.K != PointerKind::InvalidPtr; }

  /// Converts the pointer to a block pointer, if it is one.
  const BlockPointer *asBlock() const {
    return U.K == PointerKind::BlockPtr ? &U.BlockPtr : nullptr;
  }
  /// Converts the pointer to an extern pointer, if it is one.
  const ExternPointer *asExtern() const {
    return U.K == PointerKind::ExternPtr ? &U.ExternPtr : nullptr;
  }
  /// Converts the pointer to a null pointer, if it is one.
  const TargetPointer *asTarget() const {
    return U.K == PointerKind::TargetPtr ? &U.TargetPtr : nullptr;
  }

private:
  friend class Block;
  friend class DeadBlock;

  /// Storage for null pointers, pointers to blocks and pointers without data.
  ///
  /// The first field of each object must be the pointer kind to ensure that
  /// U.K return the kind of the active field.
  union Storage {
    /// Kind of the underlying storage.
    PointerKind K;
    /// Pointer to VM-allocated data.
    BlockPointer BlockPtr;
    /// Pointer to a dummy external object.
    ExternPointer ExternPtr;
    /// Pointer derived from null or a fixed address.
    TargetPointer TargetPtr;
    /// Pointer to a std::type_info structure.
    TypeInfoPointer TypeInfoPtr;

    // Constructors for block pointers.
    Storage(Block *B);
    Storage(Block *B, unsigned Base, unsigned Offset);
    Storage(BlockPointer &&Ptr);
    // Constructors for pointers to externs.
    Storage(Descriptor *Desc, const ValueDecl *VD) : ExternPtr(Desc, VD) {}
    Storage(ExternPointer &&Ptr) : ExternPtr(std::move(Ptr)) {}
    // Constructors for target pointers.
    Storage() : TargetPtr() {}
    Storage(Descriptor *Desc) : TargetPtr(Desc) {}
    Storage(Descriptor *Desc, CharUnits Offset) : TargetPtr(Desc, Offset) {}
    Storage(TargetPointer &&Ptr) : TargetPtr(std::move(Ptr)) {}
    ///Constructors for type info pointers.
    Storage(const Type *Ty, QualType InfoTy) : TypeInfoPtr(Ty, InfoTy) {}
    Storage(TypeInfoPointer &&Ptr) : TypeInfoPtr(std::move(Ptr)) {}

    // Copy and move.
    Storage(Storage &&S);
    Storage(const Storage &S);

    ~Storage();
  } U;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, const Pointer &P) {
  P.print(OS);
  return OS;
}

} // namespace interp
} // namespace clang

#endif
