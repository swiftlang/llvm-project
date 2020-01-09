// RUN: %clang_cc1 -fblocks -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -fblocks -fsyntax-only %s -verify
// expected-no-diagnostics

int compare_void_ptr(void *a, void *b) {
  return a == b;
  return a < b;
  return a > b;
}

int compare_block(void (^f)(), void (^g)()) {
  return f == g;
}

int compare_fn(void (*f)(), void (*g)()) {
  return f == g;
}

int invoke_fn(int (*f)()) {
  return (*f)();
}

class A {
  int x;
};

int compare_member(int (A::*p), int (A::*q)) {
  return p == q;
}
