// RUN: %clang_cc1 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -fsyntax-only %s -verify
// expected-no-diagnostics
// XFAIL: *

constexpr char d[2] = @encode(int); // the tree interp does not yet support this.
const char *ptr = @encode(int);
