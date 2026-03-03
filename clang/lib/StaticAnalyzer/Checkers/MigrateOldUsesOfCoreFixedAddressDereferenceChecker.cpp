//===-- MigrateOldUsesOfCoreFixedAddressDereferenceChecker.cpp ------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This is a temporary checker to make the old spelling of the checker valid.
// `core.FixedAddressDereference` was moved to
// `optin.core.FixedAddressDereference`
// Enabling the old checker name has no effect.
// Users who wish to keep the checker enabled, now need to explicitly opt-in
// by enabling this checker.
//
// This checker will be removed in the future once the migration concludes.
//
//===----------------------------------------------------------------------===//

#include "clang/StaticAnalyzer/Checkers/BuiltinCheckerRegistration.h"
#include "clang/StaticAnalyzer/Core/Checker.h"
#include "clang/StaticAnalyzer/Core/CheckerManager.h"

using namespace clang;
using namespace ento;

namespace {
class MigrateOldUsesOfCoreFixedAddressDereferenceChecker
    : public Checker<check::ASTDecl<TranslationUnitDecl>> {
public:
// Empty checker callback to make this checker valid.
void checkASTDecl(const TranslationUnitDecl *, AnalysisManager&,
                    BugReporter &) const {}
};
} // namespace

void ento::registerMigrateOldUsesOfCoreFixedAddressDereferenceChecker(CheckerManager &Mgr) {
  Mgr.registerChecker<MigrateOldUsesOfCoreFixedAddressDereferenceChecker>();
}

bool ento::shouldRegisterMigrateOldUsesOfCoreFixedAddressDereferenceChecker(const CheckerManager &) {
  return true;
}
