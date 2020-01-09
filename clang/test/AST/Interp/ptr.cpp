// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int f() {
  int a = 0;
  int &b = a;
  a = 5;
  b = 7;
  return b;
}

static_assert(f() == 7);

constexpr int &g() {
  int a = 0; // expected-note {{declared here}}
  return a;  // expected-warning {{reference to stack memory associated with local variable 'a' returned}}
}

constexpr int h() {
  return g(); // expected-note {{read of variable whose lifetime has ended}}
}

constexpr int v0 = h(); // expected-error {{constexpr variable 'v0' must be initialized by a constant expression}} \
                                                                   // expected-note {{in call to 'h()'}}

constexpr bool compare_valid_ptr() {
  int a[2] = {};
  return &a[0] < &a[1];
}

static_assert(compare_valid_ptr());

constexpr bool compare_ptr(int *a, int *b) {
  return a < b; // expected-note 3{{comparison has unspecified value}}
}

constexpr int *return_arg(int a) {
  return &a; // expected-warning {{address of stack memory associated with parameter 'a' returned}}
}

constexpr int *get_dead_ptr() {
  int a = 0;
  return &a; // expected-warning {{address of stack memory associated with local variable 'a' returned}}
}

constexpr bool compare_dead_lhs() {
  int b = 2;
  return compare_ptr(get_dead_ptr(), &b); // expected-note {{in call to 'compare_ptr(&a, &b)'}}
}

constexpr bool dead_lhs_comparison = compare_dead_lhs(); // expected-error {{constexpr variable 'dead_lhs_comparison' must be initialized by a constant expression}} \
                                                                   // expected-note {{in call to 'compare_dead_lhs()'}}

constexpr bool compare_dead_rhs() {
  int a = 2;
  return compare_ptr(&a, get_dead_ptr()); // expected-note {{in call to 'compare_ptr(&a, &a)'}}
}

constexpr bool dead_rhs_comparison = compare_dead_rhs(); // expected-error {{constexpr variable 'dead_rhs_comparison' must be initialized by a constant expression}}\
                                                                 // expected-note {{in call to 'compare_dead_rhs()'}}

constexpr bool compare_dead_lhs_rhs() {
  return compare_ptr(get_dead_ptr(), get_dead_ptr()); // expected-note {{in call to 'compare_ptr(&a, &a)'}}
}

constexpr bool dead_lhs_rhs_comparison = compare_dead_lhs_rhs(); // expected-error {{constexpr variable 'dead_lhs_rhs_comparison' must be initialized by a constant expression}}\
                                                                 // expected-note {{in call to 'compare_dead_lhs_rhs()'}}

constexpr bool compare_invalid_ptr() { // expected-error {{constexpr function never produces a constant expression}}
  int a = 0;
  int b = 1;
  return &a < &b; // expected-note 2{{comparison has unspecified value}}
}

constexpr bool v1 = compare_invalid_ptr(); // expected-error {{constexpr variable 'v1' must be initialized by a constant expression}} \
                                                                 // expected-note {{in call to 'compare_invalid_ptr()'}}

constexpr int add_callee(int *a, int *b) {
  return *a + *b;
}

constexpr int add_caller(int a, int b) {
  return add_callee(&a, &b);
}

static_assert(add_caller(2, 3) == 5);

constexpr void ptr_mutate_callee(int *a) {
  *a = 10;
}

constexpr int ptr_mutate_caller() {
  int a = 5;
  ptr_mutate_callee(&a);
  return a;
}

static_assert(ptr_mutate_caller() == 10);

constexpr void ptr_increment() { // expected-error {{constexpr function never produces a constant expression}}
  int a = 0;
  int *b = &a;
  ++b;
  ++b; // expected-note {{cannot refer to element 2 of non-array object in a constant expression}}
}

constexpr void ptr_decrement() { // expected-error {{constexpr function never produces a constant expression}}
  int a = 0;
  int *b = &a;
  --b; // expected-note {{cannot refer to element -1 of non-array object in a constant expression}}
}

constexpr int *dead_ptr_offset() {
  return get_dead_ptr() + 2;          // expected-note {{cannot refer to element 2 of non-array object in a constant expression}}
}

constexpr int *v = dead_ptr_offset(); // expected-error {{constexpr variable 'v' must be initialized by a constant expression}}\
                                      // expected-note {{in call to 'dead_ptr_offset()'}}
