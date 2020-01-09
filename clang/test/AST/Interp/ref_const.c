// RUN: %clang_cc1 -fsyntax-only -Wswitch-enum -Wcovered-switch-default -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -fsyntax-only -Wswitch-enum -Wcovered-switch-default %s -verify

/// ConstantExpr appears as an LValue here.
void test19(int i) {
  const int a = 3;
  switch (i) {
    case a: // expected-note {{previous case defined here}}
    case a: // expected-error {{duplicate case value 'a'}}
      break;
  }
}
