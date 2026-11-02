// RUN: %clang_cc1 -fsyntax-only -verify %s
// RUN: not %clang_cc1 -fsyntax-only -fdiagnostics-parseable-fixits %s 2>&1 \
// RUN:   | FileCheck %s

// The "consider using '__sized_by'" note wants the source range of the bounds
// attribute, which TypeLoc::getAsAdjusted() reaches by peeling the sugar that
// can sit between the declarator and the attribute. TypeLoc::getAs() peels none
// of it, so when the CountAttributedType is wrapped by e.g. a _Nullable
// AttributedType the attribute cannot be found by getAs() alone, the note falls
// back to the count expression and no fix-it is produced. Check that the note
// and its fix-it still land on the attribute in that case.

struct IncompleteTy; // expected-note {{consider providing a complete definition for 'struct IncompleteTy'}}

#define __counted_by(f) __attribute__((counted_by(f)))

struct Nested {
  int count;
  struct IncompleteTy *_Nullable __counted_by(count) buf; // expected-note {{consider using '__sized_by' instead of '__counted_by'}}
  // CHECK: fix-it:{{.*}}:{[[@LINE-1]]:34-[[@LINE-1]]:46}:"__sized_by"
};

void use(struct Nested *p) {
  struct IncompleteTy *l = p->buf; // expected-error{{cannot use 'p->buf' with '__counted_by' attributed type 'struct IncompleteTy * _Nullable __counted_by(count)' (aka 'struct IncompleteTy *') because the pointee type 'struct IncompleteTy' is incomplete}}
  (void)l;
}
