//===- SelfContainment.h - Self-containment inference -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the translation-unit-wide inference of whether C++ types
// are self-contained (i.e. may be treated as `Escapable` by Swift), used for
// Swift's import of C++ types.
// Results are reported through the -Wownership-inference family of diagnostics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H

namespace clang {
class ASTContext;
class Sema;
} // namespace clang

namespace clang::ownership {

/// Checks if any of the -Wownership-inference diags are enabled.
bool shouldRunSelfContainmentInference(const Sema &S);

/// Infers class-level self-containment across the whole translation unit.
void inferClassSelfContainment(ASTContext &Ctx);

} // namespace clang::ownership

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
