//===- LifetimeSummary.h - Inferred lifetimes -------------------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// A lifetime summary is a function contract for lifetime properties. It
// compensates for the lack of expressiveness in lifetime attributes.
//
// Vocabulary:
// Pointer-like type: raw pointer, C++ reference or [[gsl::Pointer]] type.
// Value(P, State): value of a pointer-like object at a state, which can be
//                  either Pre or Post for the pre- or post-state. Indirection
//                  level 0.
// RV(P, State): referent value of P (a.k.a. LValueToRValueCast(P)) at State, if
//               P is a reference to a pointer-like type. Indirection level 1.
//
// E.g., in 'void f(T* &P)', Value(P, State) is &P at State and RV(P, State) is
// the value of T* type at State.
//
// A lifetime summary is a function contract consisting of a
// 'ParameterThisSummary' for each parameter and 'this'.  For a function
// contract, there are only two states of interest: Pre and Post.
//
// A ParameterThisSummary asserts the following on a parameter or 'this',
// denoted P:
// - LifetimeBound(Level, F): the return object MAY depend on Value(P, Pre) (or
//      RV(P, Pre), depending on Level).  An optional field set F confines the
//      dependence to the subobjects of the return object selected by F.
// - CapturedBy(Level, Q, QLevel, F): Q is another parameter or 'this'.
//      Value(Q, Post) (or RV(Q, Post)) MAY depend on Value(P, Pre) (RV(P,
//      Pre)).  An optional field set F confines the dependence to the
//      subobjects of Value(Q, Post) (or RV(Q, Post)) selected by F.
// - NoEscape(Level): Value(P, Pre) (or RV(P, Pre)) MUST not escape.
// - Invalidated(F): Value(P, Pre) MAY be invalidated.  Does not apply to a C++
//      reference P, so no Level.  An optional field set F confines the
//      invalidation to the subobjects selected by F.
//
// The partial order A <= B means A is no less informative than B (it is
// concluded from no less information).
// The order only makes sense because LSA unsoundly skips unknown callees.
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMESUMMARY_H
#define LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMESUMMARY_H

#include "clang/AST/Decl.h"
#include "clang/AST/DeclCXX.h"
#include "llvm/ADT/ArrayRef.h"
#include "llvm/ADT/DenseMap.h"
#include "llvm/ADT/PointerUnion.h"
#include "llvm/ADT/SmallVector.h"
#include <functional>
#include <optional>

namespace clang::lifetimes {

/// Indirection level is limited to no indirection (Value) or take the referent
/// value from a C++ reference (ReferentValue).
enum class Level : unsigned char { Value = 0, ReferentValue = 1 };

/// A field or a base class subobject.
using FieldT = llvm::PointerUnion<const Decl *, const CXXBaseSpecifier *>;

/// A set of fields of an object (a sub-object set).  Absent means all
/// subobjects (the whole object).
///
/// Two FieldSet F and F2, F <= F2 iff F is a superset of F2. (F is no-less
/// informative than F2. Greatest is TOP, AKA unknown.)
class FieldSet {
public:
  /// Absent: all subobjects.
  FieldSet() = default;
  static FieldSet of(FieldT S);

  bool isAll() const;
  llvm::ArrayRef<FieldT> fields() const;

  void merge(const FieldSet &Other);

  /// F <= F2: F2 is absent, or both are present and F is a superset of F2.
  bool operator<=(const FieldSet &F2) const;

private:
  llvm::SmallVector<FieldT, 1> Fields;
};

/// A parameter or `this`, by its index: -1 for `this`, the index of the
/// parameter in the function otherwise.
class ParamOrThis {
public:
  static ParamOrThis forThis();
  static ParamOrThis forParam(unsigned Idx);
  static ParamOrThis forParam(const ParmVarDecl *P);

  bool isThis() const;
  /// \return the index of the parameter; not for `this`.
  unsigned getParamIndex() const;

  bool operator==(const ParamOrThis &Other) const;

private:
  friend struct llvm::DenseMapInfo<ParamOrThis>;

  explicit ParamOrThis(int Index);

  int Index;
};

} // namespace clang::lifetimes

namespace llvm {
template <> struct DenseMapInfo<clang::lifetimes::ParamOrThis> {
  using T = clang::lifetimes::ParamOrThis;
  static unsigned getHashValue(const T &V);
  static bool isEqual(const T &A, const T &B);
};
} // namespace llvm

namespace clang::lifetimes {

/// A LifetimeBoundAssertion over a parameter/this asserts that the subobjects
/// of the return object defined by \p F MAY depend on the parameter/this at
/// \p L.
struct LifetimeBoundAssertion {
  Level L;
  FieldSet F;

  bool isReferentLevel() const { return L == Level::ReferentValue; }
  bool isValueLevel() const { return L == Level::Value; }
};

/// A CapturedByAssertion over a parameter/this asserts that the subobjects
/// defined by \p F of another parameter/this \p Q at \p QL MAY depend on the
/// asserted parameter/this at \p L.
struct CapturedByAssertion {
  Level L;
  ParamOrThis Q;
  Level QL;
  FieldSet F;
};

/// Whether the users of summaries support the captured-by assertion, i.e., it
/// is at Value level on both sides. They must ignore the others. Like for
/// lifetimebound, they read every assertion as one on the whole object,
/// ignoring the field sets.
// FIXME: Support the other levels and the field sets.
inline bool isSupportedCapturedBySummary(const CapturedByAssertion &A) {
  return A.L == Level::Value && A.QL == Level::Value;
}

/// The assertions on one parameter P or on `this`.
struct ParameterThisSummary {
  llvm::SmallVector<LifetimeBoundAssertion, 1> LifetimeBound;
  llvm::SmallVector<CapturedByAssertion, 1> CapturedBy;
  /// A NoEscapeAssertion over a parameter/this asserts that the parameter/this
  /// at \p L MUST not escape.
  llvm::SmallVector<Level, 2> NoEscape;
  /// A  Invalidated assertion can only assert over pointer, not C++ reference.
  /// It asserts that a parameter/this (or sub-objects) may be invalidated.
  std::optional<FieldSet> Invalidated;
};

struct FunctionSummary {
  llvm::DenseMap<ParamOrThis, ParameterThisSummary> ParamOrThisSummaries;

  llvm::ArrayRef<LifetimeBoundAssertion>
  getLifetimeBoundFor(ParamOrThis P) const;

  llvm::ArrayRef<CapturedByAssertion> getCapturedByFor(ParamOrThis P) const;

  llvm::ArrayRef<Level> getNoEscapeFor(ParamOrThis P) const;

  /// \return the field set of the Invalidated assertion on \p P, or null.
  const FieldSet *getInvalidatedFor(ParamOrThis P) const;

  /// Prints the assertions on `this` and on each parameter of \p FD, which this
  /// is the summary of, one line each.
  void dump(llvm::raw_ostream &OS, const FunctionDecl *FD) const;
};

/// The partial order A <= B means A is no less informative than B (it is
/// concluded from no less information).
///
/// - Field sets: F <= F2 iff F2 is absent, or both are present and F is a
///   superset of F2.  Absence of a field set means all subobjects, carries the
///   least information and is the greatest.
///   (Counter-intuitive: the absence is the largest set but the least
///   informative, whereas a larger explicit set comes from more known flows.)
/// - LifetimeBound(L, F) <= LifetimeBound(L, F2) iff F <= F2; likewise
///   CapturedBy(L, Q, QL, F) <= CapturedBy(L, Q, QL, F2) iff F <= F2, and
///   Invalidated(F) <= Invalidated(F2) iff F <= F2.
/// - NoEscape assertions A, A2 (as sets): A <= A2 iff A2 is a subset of A.
///   (More NoEscape assertions is more informative.)
/// - Summaries: S <= S2 iff
///    - For each LifetimeBound, CapturedBy or Invalidated assertion A2 in S2,
///      there is an assertion A in S such that A <= A2; and
///    - NoEscape assertions in S2 are a subset of those in S.
///
/// The order only makes sense because LSA unsoundly skips unknown callees.

bool operator<=(const FunctionSummary &S, const FunctionSummary &S2);
bool operator==(const FunctionSummary &S, const FunctionSummary &S2);
bool operator<=(const ParameterThisSummary &S, const ParameterThisSummary &S2);
bool operator==(const ParameterThisSummary &S, const ParameterThisSummary &S2);

/// Builds the summary of one function. Lifetime safety analysis adds what it
/// concludes about the function as assertions in its own terms.
class FunctionSummaryBuilder {
public:
  /// Add assertion: the return object MAY depend on the value of \p P at \p L,
  /// through fields \p F.
  /// \return false, doing nothing, if the assertion already exists.
  bool addLifetimeBound(ParamOrThis P, Level L, FieldSet F = FieldSet());

  /// Add assertion: the value of \p P at \p L MAY be held by the value of \p Q
  /// at \p QL, through fields \p F.
  /// \return false, doing nothing, if the assertion already exists.
  bool addCapturedByAssertion(ParamOrThis P, Level L, ParamOrThis Q, Level QL,
                              FieldSet F = FieldSet());

  /// Add assertion: the value of \p P at \p L MUST not escape.
  /// \return false, doing nothing, if the assertion already exists.
  bool addNoEscape(ParamOrThis P, Level L);

  /// Add assertion: the pointer-like (fields \p F of) \p P MAY be invalidated.
  /// \return false, doing nothing, if the assertion already exists.
  bool addInvalidated(ParamOrThis P, FieldSet F = FieldSet());

  /// \return the summary of the function, which the builder gives up.
  FunctionSummary build();

private:
  FunctionSummary Summary;
};

/// A lookup of the summaries of the callees. A default-constructed one knows no
/// summary at all, so it is always callable.
class FunctionSummariesRef {
public:
  using LookupT = std::function<const FunctionSummary *(const FunctionDecl *)>;
  FunctionSummariesRef() = default;

  FunctionSummariesRef(LookupT Lookup) : Lookup(std::move(Lookup)) {}

  const FunctionSummary *operator()(const FunctionDecl *FD) const {
    return Lookup(FD);
  }

private:
  LookupT Lookup = [](const FunctionDecl *) -> const FunctionSummary * {
    return nullptr;
  };
};

} // namespace clang::lifetimes

#endif // LLVM_CLANG_ANALYSIS_ANALYSES_LIFETIMESAFETY_LIFETIMESUMMARY_H
