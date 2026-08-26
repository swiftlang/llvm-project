//===- unittests/CodeGen/VirtualMethodTablesTest.cpp ----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// Tests CodeGen::emitVirtualMethodTables, used by a client of CodeGen as a
// library that defines the body of a C++ virtual method itself.
//
//===----------------------------------------------------------------------===//

#include "TestCompiler.h"

#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/DeclCXX.h"
#include "clang/CodeGen/CodeGenABITypes.h"
#include "clang/CodeGen/ModuleBuilder.h"
#include "clang/Parse/ParseAST.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/GlobalVariable.h"
#include "llvm/IR/Module.h"
#include "gtest/gtest.h"

using namespace llvm;
using namespace clang;

namespace {

/// A (class name, method name) pair; the method name "~" names the destructor.
using MethodName = std::pair<StringRef, StringRef>;

/// Forwards to the CodeGenerator and, at the end of the translation unit,
/// calls emitVirtualMethodTables for the given methods, as a client that
/// defines their bodies itself would.
struct EmitTablesConsumer : public ASTConsumer {
  std::unique_ptr<CodeGenerator> Builder;
  std::vector<MethodName> Methods;

  EmitTablesConsumer(std::unique_ptr<CodeGenerator> Builder,
                     ArrayRef<MethodName> Methods)
      : Builder(std::move(Builder)), Methods(Methods) {}

  void Initialize(ASTContext &Context) override {
    Builder->Initialize(Context);
  }
  bool HandleTopLevelDecl(DeclGroupRef D) override {
    return Builder->HandleTopLevelDecl(D);
  }
  void HandleInlineFunctionDefinition(FunctionDecl *D) override {
    Builder->HandleInlineFunctionDefinition(D);
  }
  void HandleTagDeclDefinition(TagDecl *D) override {
    Builder->HandleTagDeclDefinition(D);
  }
  void HandleTagDeclRequiredDefinition(const TagDecl *D) override {
    Builder->HandleTagDeclRequiredDefinition(D);
  }
  void HandleCXXImplicitFunctionInstantiation(FunctionDecl *D) override {
    Builder->HandleCXXImplicitFunctionInstantiation(D);
  }
  void HandleCXXStaticMemberVarInstantiation(VarDecl *D) override {
    Builder->HandleCXXStaticMemberVarInstantiation(D);
  }
  void CompleteTentativeDefinition(VarDecl *D) override {
    Builder->CompleteTentativeDefinition(D);
  }
  void AssignInheritanceModel(CXXRecordDecl *RD) override {
    Builder->AssignInheritanceModel(RD);
  }
  void HandleVTable(CXXRecordDecl *RD) override { Builder->HandleVTable(RD); }

  void HandleTranslationUnit(ASTContext &Context) override {
    for (const MethodName &Name : Methods)
      CodeGen::emitVirtualMethodTables(Builder->CGM(),
                                       findMethod(Context, Name));
    Builder->HandleTranslationUnit(Context);
  }

  static const CXXMethodDecl *findMethod(ASTContext &Context,
                                         const MethodName &Name) {
    DeclContext *TU = Context.getTranslationUnitDecl();
    auto Classes = TU->lookup(&Context.Idents.get(Name.first));
    const auto *RD = cast<CXXRecordDecl>(Classes.front())->getDefinition();
    if (Name.second == "~")
      return RD->getDestructor();
    auto Methods = RD->lookup(&Context.Idents.get(Name.second));
    return cast<CXXMethodDecl>(Methods.front());
  }
};

/// Compiles \p Program and emits the tables of \p Methods into the module.
struct VirtualMethodTablesTest : public ::testing::Test {
  std::unique_ptr<TestCompiler> Compiler;
  llvm::Module *M = nullptr;

  void emit(const char *Program, ArrayRef<MethodName> Methods) {
    LangOptions LO;
    LO.CPlusPlus = 1;
    LO.CPlusPlus11 = 1;
    Compiler = std::make_unique<TestCompiler>(LO);
    auto Consumer = std::make_unique<EmitTablesConsumer>(
        std::move(Compiler->CG), Methods);
    Compiler->init(Program, std::move(Consumer));
    ParseAST(Compiler->compiler.getSema(), false, false);
    M = static_cast<EmitTablesConsumer &>(Compiler->compiler.getASTConsumer())
            .Builder->GetModule();
  }

  /// The defined global of the given name, or null if there is none.
  const GlobalValue *definition(StringRef Name) const {
    const GlobalValue *GV = M->getNamedValue(Name);
    return GV && !GV->isDeclaration() ? GV : nullptr;
  }

  /// The single defined function whose name starts with \p Prefix and ends
  /// with \p Suffix, or null if there is none.
  const Function *definedFunction(StringRef Prefix, StringRef Suffix) const {
    const Function *Found = nullptr;
    for (const Function &F : *M) {
      if (F.isDeclaration() || !F.getName().starts_with(Prefix) ||
          !F.getName().ends_with(Suffix))
        continue;
      EXPECT_EQ(Found, nullptr) << "several matches for " << Prefix;
      Found = &F;
    }
    return Found;
  }
};

TEST_F(VirtualMethodTablesTest, KeyFunctionEmitsVTableAndRTTI) {
  emit("struct S { virtual void key(); virtual void other(); };",
       {{"S", "key"}});

  const GlobalValue *VTable = definition("_ZTV1S");
  ASSERT_NE(VTable, nullptr);
  EXPECT_EQ(VTable->getLinkage(), GlobalValue::ExternalLinkage);

  const GlobalValue *TypeInfo = definition("_ZTI1S");
  ASSERT_NE(TypeInfo, nullptr);
  EXPECT_EQ(TypeInfo->getLinkage(), GlobalValue::ExternalLinkage);
  EXPECT_NE(definition("_ZTS1S"), nullptr);

  // The client defines the key function; the other method is external.
  EXPECT_NE(M->getNamedValue("_ZN1S3keyEv"), nullptr);
  EXPECT_EQ(definition("_ZN1S3keyEv"), nullptr);
  EXPECT_EQ(definition("_ZN1S5otherEv"), nullptr);
}

TEST_F(VirtualMethodTablesTest, KeyFunctionVTableIsEmittedOnce) {
  emit("struct S { virtual void key(); };", {{"S", "key"}, {"S", "key"}});

  EXPECT_NE(definition("_ZTV1S"), nullptr);
}

TEST_F(VirtualMethodTablesTest, NonKeyVirtualMethodEmitsNoVTable) {
  emit("struct S { virtual void key(); virtual void other(); };",
       {{"S", "other"}});

  EXPECT_EQ(M->getNamedValue("_ZTV1S"), nullptr);
  EXPECT_EQ(M->getNamedValue("_ZTI1S"), nullptr);
}

TEST_F(VirtualMethodTablesTest, NonVirtualMethodEmitsNothing) {
  emit("struct S { virtual void key(); void plain(); };", {{"S", "plain"}});

  EXPECT_EQ(M->getNamedValue("_ZTV1S"), nullptr);
  EXPECT_TRUE(M->empty());
}

TEST_F(VirtualMethodTablesTest, DestructorKeyFunctionEmitsVTable) {
  emit("struct S { virtual ~S(); virtual void other(); };", {{"S", "~"}});

  EXPECT_NE(definition("_ZTV1S"), nullptr);
}

TEST_F(VirtualMethodTablesTest, OverrideOfNonPrimaryBaseEmitsThisThunk) {
  emit("struct A { virtual void fa(); };"
       "struct B { virtual void fb(); };"
       "struct D : A, B { virtual void anchor(); void fb() override; };",
       {{"D", "fb"}});

  // The this-adjusting thunk in B's secondary vtable, by the size of A.
  const Function *Thunk = definedFunction("_ZThn", "_N1D2fbEv");
  ASSERT_NE(Thunk, nullptr);
  EXPECT_EQ(Thunk->getName(),
            "_ZThn" + std::to_string(Compiler->PtrSize) + "_N1D2fbEv");
  EXPECT_EQ(Thunk->getLinkage(), GlobalValue::ExternalLinkage);
  EXPECT_EQ(definition("_ZN1D2fbEv"), nullptr);

  // `anchor` is the key function; the vtable is defined with it.
  EXPECT_EQ(M->getNamedValue("_ZTV1D"), nullptr);
}

TEST_F(VirtualMethodTablesTest, CovariantOverrideEmitsReturnThunk) {
  emit("struct RA { int a; }; struct RB { int b; }; struct RC : RA, RB {};"
       "struct CB { virtual RB *clone(); };"
       "struct CD : CB { virtual void anchor(); RC *clone() override; };",
       {{"CD", "clone"}});

  // The return-adjusting thunk from RC to its RB base, by the size of RA.
  const Function *Thunk = definedFunction("_ZTch0_h", "_N2CD5cloneEv");
  ASSERT_NE(Thunk, nullptr);
  EXPECT_EQ(Thunk->getLinkage(), GlobalValue::ExternalLinkage);
}

TEST_F(VirtualMethodTablesTest, VirtualBaseEmitsVirtualThunkAndVTT) {
  emit("struct VB { int vb; virtual void vf(); };"
       "struct VD : virtual VB { virtual void anchor(); void vf() override; };",
       {{"VD", "anchor"}, {"VD", "vf"}});

  EXPECT_NE(definition("_ZTV2VD"), nullptr);
  EXPECT_NE(definition("_ZTT2VD"), nullptr);

  // The this-adjusting thunk through the virtual base's vcall offset.
  const Function *Thunk = definedFunction("_ZTv0_n", "_N2VD2vfEv");
  ASSERT_NE(Thunk, nullptr);
  EXPECT_EQ(Thunk->getLinkage(), GlobalValue::ExternalLinkage);
}

} // end anonymous namespace
