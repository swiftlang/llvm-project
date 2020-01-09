// RUN: %clang_cc1 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -fsyntax-only %s -verify
// expected-no-diagnostics

int i;
int *array[] = {&i};
