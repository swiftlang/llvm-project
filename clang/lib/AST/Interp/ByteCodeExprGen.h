//===--- ByteCodeExprGen.h - Code generator for expressions -----*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Defines the constexpr bytecode compiler.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_BYTECODEEXPRGEN_H
#define LLVM_CLANG_AST_INTERP_BYTECODEEXPRGEN_H

#include "ByteCodeEmitter.h"
#include "EvalEmitter.h"
#include "Pointer.h"
#include "PrimType.h"
#include "Record.h"
#include "clang/AST/CurrentSourceLocExprScope.h"
#include "clang/AST/Decl.h"
#include "clang/AST/Expr.h"
#include "clang/AST/StmtVisitor.h"
#include "clang/Basic/TargetInfo.h"
#include "llvm/ADT/Optional.h"

namespace clang {
class QualType;

namespace interp {
class Function;
class State;

template <class Emitter> class LocalScope;
template <class Emitter> class RecordScope;
template <class Emitter> class VariableScope;
template <class Emitter> class DeclScope;
template <class Emitter> class OptionScope;

/// Compilation context for expressions.
template <class Emitter>
class ByteCodeExprGen : public ConstStmtVisitor<ByteCodeExprGen<Emitter>, bool>,
                        public Emitter {
protected:
  // Emitters for opcodes of various arities.
  using NullaryFn = bool (ByteCodeExprGen::*)(const SourceInfo &);
  using UnaryFn = bool (ByteCodeExprGen::*)(PrimType, const SourceInfo &);
  using BinaryFn = bool (ByteCodeExprGen::*)(PrimType, PrimType,
                                             const SourceInfo &);

  // Aliases for types defined in the emitter.
  using LabelTy = typename Emitter::LabelTy;
  using AddrTy = typename Emitter::AddrTy;

  // Reference to a function generating the pointer of an initialized object.s
  using InitFnRef = std::function<bool()>;

  /// Current compilation context.
  Context &Ctx;
  /// Program to link to.
  Program &P;
  /// Execution state.
  State &S;

public:
  /// Initializes the compiler and the backend emitter.
  template <typename... Tys>
  ByteCodeExprGen(Context &Ctx, Program &P, State &S, Tys &&... Args)
      : Emitter(Ctx, P, S, Args...), Ctx(Ctx), P(P), S(S) {}

  // Expression visitors - result returned on stack.
  bool VisitDeclRefExpr(const DeclRefExpr *E);
  bool VisitCastExpr(const CastExpr *E);
  bool VisitConstantExpr(const ConstantExpr *E);
  bool VisitIntegerLiteral(const IntegerLiteral *E);
  bool VisitFloatingLiteral(const FloatingLiteral *E);
  bool VisitStringLiteral(const StringLiteral *E);
  bool VisitPredefinedExpr(const PredefinedExpr *E);
  bool VisitCharacterLiteral(const CharacterLiteral *E);
  bool VisitImaginaryLiteral(const ImaginaryLiteral *E);
  bool VisitParenExpr(const ParenExpr *E);
  bool VisitBinaryOperator(const BinaryOperator *E);
  bool VisitUnaryPostInc(const UnaryOperator *E);
  bool VisitUnaryPostDec(const UnaryOperator *E);
  bool VisitUnaryPreInc(const UnaryOperator *E);
  bool VisitUnaryPreDec(const UnaryOperator *E);
  bool VisitUnaryAddrOf(const UnaryOperator *E);
  bool VisitUnaryDeref(const UnaryOperator *E);
  bool VisitUnaryPlus(const UnaryOperator *E);
  bool VisitUnaryMinus(const UnaryOperator *E);
  bool VisitUnaryNot(const UnaryOperator *E);
  bool VisitUnaryLNot(const UnaryOperator *E);
  bool VisitUnaryReal(const UnaryOperator *E);
  bool VisitUnaryImag(const UnaryOperator *E);
  bool VisitUnaryExtension(const UnaryOperator *E);
  bool VisitConditionalOperator(const ConditionalOperator *E);
  bool VisitBinaryConditionalOperator(const BinaryConditionalOperator *E);
  bool VisitMemberExpr(const MemberExpr *E);
  bool VisitCallExpr(const CallExpr *E);
  bool VisitArraySubscriptExpr(const ArraySubscriptExpr *E);
  bool VisitExprWithCleanups(const ExprWithCleanups *E);
  bool VisitCompoundLiteralExpr(const CompoundLiteralExpr *E);
  bool VisitMaterializeTemporaryExpr(const MaterializeTemporaryExpr *E);
  bool VisitImplicitValueInitExpr(const ImplicitValueInitExpr *E);
  bool VisitSubstNonTypeTemplateParmExpr(const SubstNonTypeTemplateParmExpr *E);
  bool VisitUnaryExprOrTypeTraitExpr(const UnaryExprOrTypeTraitExpr *E);
  bool VisitChooseExpr(const ChooseExpr *E);
  bool VisitGenericSelectionExpr(const GenericSelectionExpr *E);
  bool VisitOffsetOfExpr(const OffsetOfExpr *OOE);
  bool VisitSizeOfPackExpr(const SizeOfPackExpr *E);
  bool VisitOpaqueValueExpr(const OpaqueValueExpr *E);
  bool VisitArrayInitLoopExpr(const ArrayInitLoopExpr *E);
  bool VisitArrayInitIndexExpr(const ArrayInitIndexExpr *E);
  bool VisitInitListExpr(const InitListExpr *E);
  bool VisitTypeTraitExpr(const TypeTraitExpr *E);
  bool VisitBlockExpr(const BlockExpr *E);
  bool VisitGNUNullExpr(const GNUNullExpr *E);
  bool VisitLambdaExpr(const LambdaExpr *E);
  bool VisitSourceLocExpr(const SourceLocExpr *E);
  bool VisitAddrLabelExpr(const AddrLabelExpr *E);
  bool VisitConceptSpecializationExpr(const ConceptSpecializationExpr *E);
  bool VisitCXXConstructExpr(const CXXConstructExpr *E);
  bool VisitCXXMemberCallExpr(const CXXMemberCallExpr *E);
  bool VisitCXXNullPtrLiteralExpr(const CXXNullPtrLiteralExpr *E);
  bool VisitCXXBoolLiteralExpr(const CXXBoolLiteralExpr *E);
  bool VisitCXXThisExpr(const CXXThisExpr *E);
  bool VisitCXXDefaultArgExpr(const CXXDefaultArgExpr *E);
  bool VisitCXXDefaultInitExpr(const CXXDefaultInitExpr *E);
  bool VisitCXXScalarValueInitExpr(const CXXScalarValueInitExpr *E);
  bool VisitCXXThrowExpr(const CXXThrowExpr *E);
  bool VisitCXXTemporaryObjectExpr(const CXXTemporaryObjectExpr *E);
  bool VisitCXXTypeidExpr(const CXXTypeidExpr *E);
  bool VisitCXXReinterpretCastExpr(const CXXReinterpretCastExpr *E);
  bool VisitCXXDynamicCastExpr(const CXXDynamicCastExpr *E);
  bool VisitCXXInheritedCtorInitExpr(const CXXInheritedCtorInitExpr *E);
  bool VisitCXXStdInitializerListExpr(const CXXStdInitializerListExpr *E);
  bool VisitCXXNewExpr(const CXXNewExpr *E);
  bool VisitCXXNoexceptExpr(const CXXNoexceptExpr *E);
  bool VisitCXXBindTemporaryExpr(const CXXBindTemporaryExpr *E);
  bool VisitObjCBoolLiteralExpr(const ObjCBoolLiteralExpr *E);
  bool VisitObjCStringLiteral(const ObjCStringLiteral *E);
  bool VisitObjCEncodeExpr(const ObjCEncodeExpr *E);
  bool VisitObjCBoxedExpr(const ObjCBoxedExpr *E);
  bool VisitObjCIvarRefExpr(const ObjCIvarRefExpr *E);

protected:
  bool visitExpr(const Expr *E) override;
  bool visitRValue(const Expr *E) override;
  bool visitDecl(const VarDecl *VD) override;

protected:
  /// Emits scope cleanup instructions.
  void emitCleanup();

  /// Returns a record type from a record or pointer type.
  const RecordType *getRecordTy(QualType Ty);

  /// Returns a record from a record or pointer type.
  Record *getRecord(QualType Ty);
  Record *getRecord(const RecordDecl *RD);

  /// Perform an action if a record field is found.
  bool withField(const FieldDecl *F,
                 llvm::function_ref<bool(const Record::Field *)> GenField);

  /// Returns the size int bits of an integer.
  unsigned getIntWidth(QualType Ty) {
    return Ctx.getASTContext().getIntWidth(Ty);
  }
  /// Returns the applicable fp semantics.
  const fltSemantics *getFltSemantics(QualType Ty) {
    return &Ctx.getASTContext().getFloatTypeSemantics(Ty);
  }
  /// Returns the value of CHAR_BIT.
  unsigned getCharBit() const {
    return Ctx.getASTContext().getTargetInfo().getCharWidth();
  }

  /// Canonicalizes an array type.
  const ConstantArrayType *getAsConstantArrayType(QualType AT) {
    return Ctx.getASTContext().getAsConstantArrayType(AT);
  }

  /// Classifies a type.
  llvm::Optional<PrimType> classify(const Expr *E) const;
  llvm::Optional<PrimType> classify(QualType Ty) const {
    return Ctx.classify(Ty);
  }

  /// Classifies a known primitive type
  PrimType classifyPrim(QualType Ty) const {
    if (auto T = classify(Ty)) {
      return *T;
    }
    llvm_unreachable("not a primitive type");
  }

  /// Evaluates an expression for side effects and discards the result.
  bool discard(const Expr *E);
  /// Evaluates an expression and places result on stack.
  bool visit(const Expr *E);
  /// Compiles an initializer for a local.
  bool visitInitializer(const Expr *E, InitFnRef GenPtr);

  /// Visits an expression and converts it to a boolean.
  bool visitBool(const Expr *E);

  /// Visits a base class initializer.
  bool visitBaseInitializer(const Expr *Init, InitFnRef GenBase);

  /// Visits an initializer for a local.
  bool visitLocalInitializer(const Expr *Init, unsigned I) {
    return visitInitializer(
        Init, [this, I, Init] { return this->emitGetPtrLocal(I, Init); });
  }

  /// Visits an initializer for a global.
  bool visitGlobalInitializer(const Expr *Init, GlobalLocation I) {
    return visitInitializer(
        Init, [this, I, Init] { return this->emitGetPtrGlobal(I, Init); });
  }

  /// Visits a delegated initializer.
  bool visitThisInitializer(const Expr *I) {
    return visitInitializer(I, [this, I] { return this->emitThis(I); });
  }

  /// Creates a local primitive value.
  unsigned allocateLocalPrimitive(DeclTy &&Decl, QualType Ty, PrimType Type,
                                  bool IsConst, bool IsExtended = false);

  /// Allocates a space storing a local given its type.
  llvm::Optional<unsigned> allocateLocal(DeclTy &&Decl, QualType Ty,
                                         bool IsExtended = false);

  /// Emits a dummy pointer.
  bool emitDummyPtr(QualType Ty, const VarDecl *VD, const Expr *E);
  /// Emits a dummy void pointer.
  bool emitDummyVoidPtr(QualType Ty, const VarDecl *VD, const Expr *E);

private:
  friend class VariableScope<Emitter>;
  friend class LocalScope<Emitter>;
  friend class RecordScope<Emitter>;
  friend class DeclScope<Emitter>;
  friend class OptionScope<Emitter>;

  /// Emits a zero initializer.
  bool visitZeroInitializer(PrimType T, const Expr *E);

  /// Emits a check to validate function before arguments.
  bool checkFunctionCall(const FunctionDecl *Callee, const Expr *Call);
  /// Emits a direct function call.
  bool emitFunctionCall(const FunctionDecl *Callee, llvm::Optional<PrimType> T,
                        const Expr *Call);

  /// Emits a check to validate function before arguments.
  bool checkMethodCall(const CXXMethodDecl *Callee, const Expr *Call);
  /// Emits a direct method call.
  bool emitMethodCall(const CXXMethodDecl *Callee, llvm::Optional<PrimType> T,
                      const Expr *Call);

  /// Compiles an offset calculator.
  bool visitOffset(const Expr *Ptr, const Expr *Offset, const Expr *E,
                   PrimType PT, UnaryFn OffsetFn);

  /// Fetches a member of a structure given by a pointer.
  bool visitIndirectMember(const BinaryOperator *E);

  /// Spaceship operator.
  bool visitSpaceship(const BinaryOperator *E);

  /// Compiles  simple or compound assignments.
  bool visitAssign(PrimType T, const BinaryOperator *BO);
  bool visitShiftAssign(PrimType RHS, BinaryFn F, const BinaryOperator *BO);
  bool visitCompoundAssign(PrimType RHS, UnaryFn F, const BinaryOperator *BO);
  bool visitPtrAssign(PrimType RHS, UnaryFn F, const BinaryOperator *BO);

  /// Emits a cast between two types.
  bool emitConv(PrimType From, QualType FromTy, PrimType To, QualType ToTy,
                const Expr *Cast);

  bool visitShortCircuit(const BinaryOperator *E);

  enum class DerefKind {
    /// Value is read and pushed to stack.
    Read,
    /// Direct method generates a value which is written. Returns pointer.
    Write,
    /// Direct method receives the value, pushes mutated value. Returns pointer.
    ReadWrite,
  };

  /// Method to directly load a value. If the value can be fetched directly,
  /// the direct handler is called. Otherwise, a pointer is left on the stack
  /// and the indirect handler is expected to operate on that.
  bool dereference(const Expr *LV, DerefKind AK,
                   llvm::function_ref<bool(PrimType)> Direct,
                   llvm::function_ref<bool(PrimType)> Indirect);
  bool dereferenceParam(const DeclRefExpr *LV, PrimType T,
                        const ParmVarDecl *PD, DerefKind AK,
                        llvm::function_ref<bool(PrimType)> Direct,
                        llvm::function_ref<bool(PrimType)> Indirect);
  bool dereferenceVar(const DeclRefExpr *LV, PrimType T, const VarDecl *PD,
                      DerefKind AK, llvm::function_ref<bool(PrimType)> Direct,
                      llvm::function_ref<bool(PrimType)> Indirect);
  bool dereferenceMember(const MemberExpr *PD, PrimType T, DerefKind AK,
                         llvm::function_ref<bool(PrimType)> Direct,
                         llvm::function_ref<bool(PrimType)> Indirect);
  bool dereferenceLambda(const DeclRefExpr *DE, const FieldDecl *FD, PrimType T,
                         DerefKind AK,
                         llvm::function_ref<bool(PrimType)> Direct,
                         llvm::function_ref<bool(PrimType)> Indirect);

  /// Converts an lvalue to an rvalue.
  bool lvalueToRvalue(const Expr *LV, const Expr *E);

  /// Emits an APInt constant.
  bool emitConst(PrimType T, unsigned NumBits, const llvm::APInt &Value,
                 const Expr *E);
  /// Tries to emit a value from an APValue.
  bool emitValue(PrimType T, APValue &Value, const Expr *E);

  /// Emits an integer constant.
  template <typename T> bool emitConst(const Expr *E, T Value) {
    QualType Ty = E->getType();
    unsigned NumBits = getIntWidth(Ty);
    APInt WrappedValue(NumBits, Value, std::is_signed<T>::value);
    return emitConst(*Ctx.classify(Ty), NumBits, WrappedValue, E);
  }

  /// Compiles a list of arguments.
  bool visitArguments(QualType CalleeTy, ArrayRef<const Expr *> Args);
  /// Compiles an argument.
  bool visitArgument(const Expr *E, bool Discard);

  /// Visits a constant function invocation.
  bool getPtrConstFn(const FunctionDecl *FD, const Expr *E);
  /// Returns a pointer to a variable declaration.
  bool getPtrVarDecl(const VarDecl *VD, const Expr *E);

  /// Compiles a string initializer.
  bool visitStringInitializer(const StringLiteral *S);
  /// Compiles an array initializer.
  bool visitArrayInitializer(const ConstantArrayType *AT, const Expr *E,
                             llvm::function_ref<const Expr *(uint64_t)> Elem);
  /// Visits a record initializer.
  bool visitRecordInitializer(const RecordType *RT, const InitListExpr *List);
  /// Visits a record field initializer.
  bool visitRecordFieldInitializer(Record *R, const Record::Field *F,
                                   const Expr *E);
  /// Visits a complex initializer.
  bool visitComplexInitializer(const ComplexType *CT, const InitListExpr *List);
  /// Visits a vector initializer.
  bool visitVectorInitializer(const VectorType *VT, const InitListExpr *List);

  /// Visits a cast from scalar to complex.
  bool visitCastToComplex(const CastExpr *CE);
  /// Visits a cast from complex to scalar.
  bool visitCastToReal(const CastExpr *CE);

  /// Zero initializer for a record.
  bool visitZeroInitializer(const Expr *E, const RecordDecl *RD);

  /// Registers an opaque expression.
  bool visitOpaqueExpr(const OpaqueValueExpr *Expr);
  /// Visits a conditional operator.
  bool visitConditionalOperator(const AbstractConditionalOperator *CO);

  /// Materializes a composite.
  bool materialize(const Expr *Alloc, const Expr *Init, bool IsConst,
                   bool IsGlobal, bool IsExtended);

  /// Emits the initialized pointer.
  bool emitInitFn() {
    return InitFn ? (*InitFn)() : this->emitTrap(SourceInfo{});
  }

protected:
  /// Variable to storage mapping.
  llvm::DenseMap<const ValueDecl *, Scope::Local> Locals;

  /// OpaqueValueExpr to location mapping.
  llvm::DenseMap<const OpaqueValueExpr *, unsigned> OpaqueExprs;

  /// Current scope.
  VariableScope<Emitter> *VarScope = nullptr;

  /// Current argument index.
  llvm::Optional<uint64_t> ArrayIndex;

  /// Flag indicating if return value is to be discarded.
  bool DiscardResult = false;

  /// Expression being initialized.
  llvm::Optional<InitFnRef> InitFn = {};

  /// Enumeration of initializer kinds.
  enum class InitKind {
    /// Regular invocation.
    ROOT,
    /// Base class initializer.
    BASE,
    /// Activates a union field.
    UNION,
  };

  /// Initialiser kinds for the current object.
  InitKind Initialiser = InitKind::ROOT;

  /// Mapping from var decls to captured fields.
  llvm::DenseMap<const VarDecl *, FieldDecl *> CaptureFields;
  /// Override for 'this' captured by lambdas.
  llvm::Optional<FieldDecl *> CaptureThis;

  /// Declaration being evaluated.
  const VarDecl *EvaluatingDecl = nullptr;

  /// Source location information for the default argument or initializer.
  CurrentSourceLocExprScope SourceLocScope;
};

extern template class ByteCodeExprGen<ByteCodeEmitter>;
extern template class ByteCodeExprGen<EvalEmitter>;

/// Scope chain managing the variable lifetimes.
template <class Emitter> class VariableScope {
public:
  virtual ~VariableScope() { Ctx->VarScope = this->Parent; }

  void add(const Scope::Local &Local, bool IsExtended) {
    if (IsExtended)
      this->addExtended(Local);
    else
      this->addLocal(Local);
  }

  virtual void addLocal(const Scope::Local &Local) {
    if (this->Parent)
      this->Parent->addLocal(Local);
  }

  virtual void addExtended(const Scope::Local &Local) {
    if (this->Parent)
      this->Parent->addExtended(Local);
  }

  virtual void emitDestruction() {}

  VariableScope *getParent() { return Parent; }

protected:
  VariableScope(ByteCodeExprGen<Emitter> *Ctx)
      : Ctx(Ctx), Parent(Ctx->VarScope) {
    Ctx->VarScope = this;
  }

  /// ByteCodeExprGen instance.
  ByteCodeExprGen<Emitter> *Ctx;
  /// Link to the parent scope.
  VariableScope *Parent;
};

/// Scope for local variables.
///
/// When the scope is destroyed, instructions are emitted to tear down
/// all variables declared in this scope.
template <class Emitter> class LocalScope : public VariableScope<Emitter> {
public:
  LocalScope(ByteCodeExprGen<Emitter> *Ctx) : VariableScope<Emitter>(Ctx) {}

  ~LocalScope() override { this->emitDestruction(); }

  void addLocal(const Scope::Local &Local) override {
    if (!Idx.hasValue()) {
      Idx = this->Ctx->Descriptors.size();
      this->Ctx->Descriptors.emplace_back();
    }

    this->Ctx->Descriptors[*Idx].emplace_back(Local);
  }

  void emitDestruction() override {
    if (!Idx.hasValue())
      return;
    this->Ctx->emitDestroy(*Idx, SourceInfo{});
  }

protected:
  /// Index of the scope in the chain.
  Optional<unsigned> Idx;
};

/// Scope for storage declared in a compound statement.
template <class Emitter> class BlockScope final : public LocalScope<Emitter> {
public:
  BlockScope(ByteCodeExprGen<Emitter> *Ctx) : LocalScope<Emitter>(Ctx) {}

  void addExtended(const Scope::Local &Local) override {
    llvm_unreachable("Cannot create temporaries in full scopes");
  }
};

/// Expression scope which tracks potentially lifetime extended
/// temporaries which are hoisted to the parent scope on exit.
template <class Emitter> class ExprScope final : public LocalScope<Emitter> {
public:
  ExprScope(ByteCodeExprGen<Emitter> *Ctx) : LocalScope<Emitter>(Ctx) {}

  void addExtended(const Scope::Local &Local) override {
    if (this->Parent)
      this->Parent->addLocal(Local);
    else
      this->addLocal(Local);
  }
};

} // namespace interp
} // namespace clang

#endif
