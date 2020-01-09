//===--- Program.h - Bytecode for the constexpr VM --------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines a program which organises and links multiple bytecode functions.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_PROGRAM_H
#define LLVM_CLANG_AST_INTERP_PROGRAM_H

#include "Function.h"
#include "Pointer.h"
#include "PrimType.h"
#include "Record.h"
#include "Source.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/StringRef.h"
#include "llvm/Support/Allocator.h"
#include <map>
#include <vector>

namespace clang {
class RecordDecl;
class Expr;
class FunctionDecl;
class Stmt;
class StringLiteral;
class VarDecl;

namespace interp {
class Context;
class State;
class Record;
class Scope;
class Program;

/// Type used to reference a global.
class GlobalLocation {
public:
  GlobalLocation() : Index(static_cast<unsigned>(-1)) {}
  GlobalLocation(unsigned Index) : Index(Index) {}

  void print(llvm::raw_ostream &OS) { OS << "{" << Index << "}"; }

private:
  friend class Program;
  unsigned Index;
};

inline llvm::raw_ostream &operator<<(llvm::raw_ostream &OS, GlobalLocation G) {
  G.print(OS);
  return OS;
}

/// The program contains and links the bytecode for all functions.
class Program {
public:
  Program(Context &Ctx) : Ctx(Ctx) {}

  /// Emits a string literal as a global block.
  unsigned createGlobalString(const Expr *E, const StringLiteral *S);
  /// Emits a string literal as a global block.
  unsigned createGlobalString(StringRef Str);

  /// Returns a pointer to a global.
  Pointer getPtrGlobal(GlobalLocation Idx) { return getGlobal(Idx); }
  /// Returns  a pointer to a global by index.
  Pointer getPtrGlobal(const VarDecl *VD);

  /// Returns the value of a global.
  Block *getGlobal(GlobalLocation Idx);

  /// Returns a global if it was created.
  llvm::Optional<GlobalLocation> getGlobal(const VarDecl *VD);

  /// Creates a global and returns its index.
  llvm::Optional<GlobalLocation> createGlobal(const VarDecl *VD);
  /// Prepares a global, but does not allocate storage.
  llvm::Optional<GlobalLocation> prepareGlobal(const VarDecl *VD);

  /// Creates a global from a lifetime-extended temporary with a different type.
  llvm::Optional<GlobalLocation> createGlobal(const Expr *E, QualType Ty);
  /// Creates a global with a specific type.
  llvm::Optional<GlobalLocation> createGlobal(const Expr *E) {
    return createGlobal(E, E->getType());
  }

  /// Removes a global whose evaluation failed.
  void removeGlobal(const VarDecl *VD);

  /// Creates a new function from a code range.
  template <typename... Ts>
  Function *createFunction(const FunctionDecl *Def, Ts &&... Args) {
    auto *Func = new Function(*this, Def, std::forward<Ts>(Args)...);
    Funcs.insert({Def, std::unique_ptr<Function>(Func)});
    return Func;
  }
  /// Creates an anonymous function.
  template <typename... Ts> Function *createFunction(Ts &&... Args) {
    auto *Func = new Function(*this, std::forward<Ts>(Args)...);
    AnonFuncs.emplace_back(Func);
    return Func;
  }

  /// Returns a function.
  Function *getFunction(const FunctionDecl *F);

  /// Returns a pointer to a function if it exists and can be compiled.
  /// If a function couldn't be compiled, an error is returned.
  /// If a function was not yet defined, a null pointer is returned.
  llvm::Expected<Function *> getOrCreateFunction(State &S,
                                                 const FunctionDecl *F);

  /// Returns a record or creates one if it does not exist.
  Record *getOrCreateRecord(const RecordDecl *RD);

  /// Creates a descriptor for a primitive type.
  Descriptor *createDescriptor(const DeclTy &D, const Type *Ty, PrimType Type,
                               bool IsConst = false, bool IsTemporary = false,
                               bool IsMutable = false);

  /// Creates a descriptor for a composite type.
  Descriptor *createDescriptor(const DeclTy &D, const Type *Ty,
                               bool IsConst = false, bool IsTemporary = false,
                               bool IsMutable = false);

  /// Creates a descriptor for a composite or primitive type.
  Descriptor *createDescriptor(const DeclTy &D, QualType Ty);

  /// Context to manage declaration lifetimes.
  class DeclScope {
  public:
    DeclScope(Program &P, const VarDecl *VD) : P(P) { P.startDeclaration(VD); }
    ~DeclScope() { P.endDeclaration(); }

  private:
    Program &P;
  };

  /// Returns the current declaration ID.
  llvm::Optional<unsigned> getCurrentDecl() const {
    if (CurrentDeclaration == NoDeclaration)
      return llvm::Optional<unsigned>{};
    return LastDeclaration;
  }

private:
  friend class DeclScope;

  /// Reference to the VM context.
  Context &Ctx;
  /// Mapping from decls to cached bytecode functions.
  llvm::DenseMap<const FunctionDecl *, std::unique_ptr<Function>> Funcs;
  /// List of anonymous functions.
  std::vector<std::unique_ptr<Function>> AnonFuncs;

  /// Function relocation locations.
  llvm::DenseMap<const FunctionDecl *, std::vector<unsigned>> Relocs;

  /// Custom allocator for global storage.
  using PoolAllocTy = llvm::BumpPtrAllocatorImpl<llvm::MallocAllocator>;

  /// Descriptor + storage for a global object.
  ///
  /// Global objects never go out of scope, thus they do not track pointers.
  class Global {
  public:
    /// Create a global descriptor for string literals.
    template <typename... Tys>
    Global(Tys... Args) : B(std::forward<Tys>(Args)...) {}

    /// Allocates the global in the pool, reserving storate for data.
    void *operator new(size_t Meta, PoolAllocTy &Alloc, size_t Data) {
      return Alloc.Allocate(Meta + Data, alignof(void *));
    }

    /// Return a pointer to the data.
    char *data() { return B.data(); }
    /// Return a pointer to the block.
    Block *block() { return &B; }

  private:
    /// Required metadata - does not actually track pointers.
    Block B;
  };

  llvm::Optional<GlobalLocation> createGlobal(const DeclTy &D, QualType Ty,
                                              bool IsStatic);

  Global *allocateGlobal(const DeclTy &D, QualType Ty, bool IsStatic);

  /// Returns an initialised global block.
  Block *getGlobalBlock(GlobalLocation Loc);

  /// Allocator for globals.
  PoolAllocTy Allocator;

  /// Global objects.
  std::vector<llvm::PointerUnion<Global *, const VarDecl *>> Globals;
  /// Cached global indices.
  llvm::DenseMap<const void *, unsigned> GlobalIndices;

  /// Mapping from decls to record metadata.
  llvm::DenseMap<const RecordDecl *, Record *> Records;

  /// Creates a new descriptor.
  template <typename... Ts> Descriptor *allocateDescriptor(Ts &&... Args) {
    return new (Allocator) Descriptor(std::forward<Ts>(Args)...);
  }

  /// No declaration ID.
  static constexpr unsigned NoDeclaration = (unsigned)-1;
  /// Last declaration ID.
  unsigned LastDeclaration = 0;
  /// Current declaration ID.
  unsigned CurrentDeclaration = NoDeclaration;

  /// Starts evaluating a declaration.
  void startDeclaration(const VarDecl *Decl) {
    LastDeclaration += 1;
    CurrentDeclaration = LastDeclaration;
  }

  /// Ends a global declaration.
  void endDeclaration() { CurrentDeclaration = NoDeclaration; }

public:
  /// Dumps the disassembled bytecode to \c llvm::errs().
  void dump() const;
  void dump(llvm::raw_ostream &OS) const;
};

} // namespace interp
} // namespace clang

#endif
