//===- SelfContainment.h - Self-containment inference -----------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file declares the translation-unit-wide self-containment inference.
//
// An object is self-contained if it owns every object reachable from it.
// A self-contained object:
//  - never goes corrupt during its lifetime,
//    i.e. it never holds dangling pointers or references, and
//  - if it can be copied, then its copies are separate from it.
// A type is self-contained if all objects of that type are self-contained.
// Otherwise, it's a view.
// Swift imports self-contained types as `Escapable`.
//
// A class is self-contained if:
//  - each of its bases is self-contained, and
//  - each of its fields is either:
//    - a self-contained value, or
//    - an owned pointer to a self-contained pointee.
// An owned pointer is copied, moved and destroyed together with the class.
// Reference counted pointers are owned too.
//
// Results are reported through the -Wtype-ownership family of diagnostics.
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H

namespace clang {
class ASTContext;
class DiagnosticsEngine;
} // namespace clang

namespace clang::ownership {

/// Checks if any of the -Wtype-ownership diags are enabled.
bool shouldRunSelfContainmentInference(const DiagnosticsEngine &Diags);

/// Infers class-level self-containment across the whole translation unit.
void inferClassSelfContainment(ASTContext &Ctx);

} // namespace clang::ownership

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_OWNERSHIP_SELFCONTAINMENT_H
