// RUN: %clang_cc1 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -fsyntax-only %s -verify
// expected-no-diagnostics

extern int g();

void test()
{
  int cond;
  switch (cond) {
  case 0 && g():
    break;
  }
}
