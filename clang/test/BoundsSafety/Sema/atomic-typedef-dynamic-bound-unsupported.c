// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify %s

// A bounds attribute on an `_Atomic` pointer is not supported, including when
// the `_Atomic(...)` is reached through a typedef. ValidateBoundsAttrTypeShape
// walks sugar (`Ty->getAs<AtomicType>()`) to find the AtomicType behind the
// typedef, so the _Atomic-specific diagnostic fires. A top-node-only check would
// miss the typedef and fall through to the generic "only applies to pointers"
// diagnostic instead.
//
// The typedef is written with an explicit `__single` so that -fbounds-safety
// auto-bounding leaves the underlying type unchanged and therefore retains the
// typedef (see MakeAutoPointer::VisitTypedefType). Without the explicit
// `__single`, auto-bounding rewrites the underlying pointer and drops the
// typedef, so the type reaching the check is already a bare AtomicType and the
// sugar-walking is not exercised.
//
// The count/end sibling is declared before the attributed field so the
// attribute is resolved on the eager path (ConstructDynamicBoundType), where
// this check runs.

#include <ptrcheck.h>

typedef _Atomic(int *__single) atomic_int_ptr_t;

struct counted {
  int n;
  // expected-error@+1{{_Atomic on '__counted_by' pointer is not yet supported}}
  atomic_int_ptr_t p __counted_by(n);
};

struct counted_or_null {
  int n;
  // expected-error@+1{{_Atomic on '__counted_by_or_null' pointer is not yet supported}}
  atomic_int_ptr_t p __counted_by_or_null(n);
};

struct sized {
  int n;
  // expected-error@+1{{_Atomic on '__sized_by' pointer is not yet supported}}
  atomic_int_ptr_t p __sized_by(n);
};

struct sized_or_null {
  int n;
  // expected-error@+1{{_Atomic on '__sized_by_or_null' pointer is not yet supported}}
  atomic_int_ptr_t p __sized_by_or_null(n);
};

struct ended {
  int *e;
  // expected-error@+1{{_Atomic on '__ended_by' pointer is not yet supported}}
  atomic_int_ptr_t p __ended_by(e);
};
