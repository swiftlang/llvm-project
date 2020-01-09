// RUN: %clang_cc1 -triple=x86_64-apple-darwin12 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -triple=x86_64-apple-darwin12 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics


static_assert(alignof(void *) == 8);
