// A bounds attribute on a block literal's return type is currently accepted and
// then silently dropped, so nothing is diagnosed here and no return size check
// is emitted (rdar://132927229). Once the parser wiring is added, a block
// return type will carry a bounds attribute and the assertion in
// CheckCountAttributedDeclAssignments::TraverseReturnStmt will fire, at which
// point this test needs updating together with that code.

// RUN: %clang_cc1 -fsyntax-only -fblocks -fbounds-safety -verify %s
// RUN: %clang_cc1 -fsyntax-only -fblocks -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -verify %s

#include <ptrcheck.h>

// expected-no-diagnostics

typedef unsigned long size_t;

// __counted_by referring to a block parameter.
void test_counted_by_param(void) {
  int *(^b)(int n) = ^int *__counted_by(n)(int n) { return 0; };
  (void)b;
}

// __counted_by with a constant count, so the count does not depend on late
// parsing of the parameter list.
void test_counted_by_constant(void) {
  int *(^b)(void) = ^int *__counted_by(4)(void) { return 0; };
  (void)b;
}

void test_counted_by_or_null(void) {
  int *(^b)(int n) = ^int *__counted_by_or_null(n)(int n) { return 0; };
  (void)b;
}

void test_sized_by(void) {
  void *(^b)(size_t n) = ^void *__sized_by(n)(size_t n) { return 0; };
  (void)b;
}

// __ended_by builds a DynamicRangePointerType rather than a
// CountAttributedType, and is dropped the same way.
void test_ended_by(void) {
  void *(^b)(void *__single e) = ^void *__ended_by(e)(void *__single e) {
    return e;
  };
  (void)b;
}

// A block with the parameter list omitted, so the signature as written is the
// bare return type rather than a function type. That is the shape the
// assertion's helper has to handle separately.
void test_omitted_param_list(void) {
  int *(^b)(void) = ^int *__counted_by(4) { return 0; };
  (void)b;
}
