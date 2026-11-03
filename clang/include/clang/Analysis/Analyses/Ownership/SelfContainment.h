//===- SelfContainment.h - Self-containment inference -----------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the translation-unit-wide inference of whether C++ types
// are self-contained (i.e. may be treated as `Escapable` by Swift), used for
// Swift's import of C++ types. Results are reported through the
// -Wownership-inference diagnostics.
//
// The inference depends on the lifetime safety analysis only through the
// LifetimeSafetyObserver extension point: a DestroyedFieldsCollector is
// registered with the per-function analysis, and consumed once afterwards.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H

#include "clang/Analysis/Analyses/LifetimeSafety/Observer.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/DenseSet.h"

namespace clang {
class ASTContext;
class CXXRecordDecl;
class FieldDecl;
} // namespace clang

namespace clang::ownership {

/// Accumulates, for each class whose destructor was analyzed, the set of
/// pointer fields the destructor was seen to destroy. Owned by the TU driver,
/// registered as the lifetime safety observer for every per-function pass, and
/// consumed by inferClassSelfContainment.
class DestroyedFieldsCollector final
    : public lifetimes::LifetimeSafetyObserver {
public:
  void onDestructorDestroysField(const CXXRecordDecl *Class,
                                 const FieldDecl *Field) override;

  /// The fields of \p Class its destructor was seen to destroy; empty if none.
  const llvm::DenseSet<const FieldDecl *> &
  getDestroyedFields(const CXXRecordDecl *Class) const;

private:
  llvm::DenseMap<const CXXRecordDecl *, llvm::DenseSet<const FieldDecl *>> Map;
};

/// Infers class-level self-containment across the whole translation unit. Must
/// be called once, after all per-function lifetime safety analyses have run, so
/// \p Collector (populated by each destructor's pass) is complete.
void inferClassSelfContainment(ASTContext &Ctx,
                               const DestroyedFieldsCollector &Collector);

} // namespace clang::ownership

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
