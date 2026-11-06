//===- Observer.h - Lifetime safety analysis observer -----------*- C++-*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines LifetimeSafetyObserver, the extension point through which
// clients layered on top of the lifetime safety analysis (e.g. the ownership
// analyses) can learn about facts the checker derives, without depending on the
// analysis' internal representation (facts, loans and origins).
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_OBSERVER_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_OBSERVER_H

namespace clang {
class CXXRecordDecl;
class FieldDecl;
} // namespace clang

namespace clang::lifetimes {

/// Receives notifications from the lifetime safety checker as it analyzes
/// function bodies. Implemented by clients; the analysis never depends on them.
class LifetimeSafetyObserver {
public:
  LifetimeSafetyObserver() = default;
  virtual ~LifetimeSafetyObserver() = default;

  /// The destructor of \p Class was seen to destroy the pointee of its pointer
  /// field \p Field, i.e. the `$this.field` loan was invalidated by a
  /// destroying invalidation. The link travels through the loan, not the
  /// syntax, so it also fires when the field flows through a local alias
  /// (`int *z = m_ptr; consume(z);`).
  virtual void onDestructorDestroysField(const CXXRecordDecl *Class,
                                         const FieldDecl *Field) {}
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_OBSERVER_H
