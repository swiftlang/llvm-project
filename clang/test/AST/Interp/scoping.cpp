// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter -verify %s

// Clang emits a diagnostic treating 'a' as a subobject, while the interpreter
// emits what we think is the appropriate diagnostic for local variables.

constexpr int scope_escape() { // expected-error {{constexpr function never produces a constant expression}}
  int c = 0;
  int *a = &c;
  {
    int b = 0; // expected-note {{declared here}} \
                                            // expected-note {{declared here}}
    a = &b;
  }
  return *a; // expected-note {{read of variable whose lifetime has ended}}\
                                            // expected-note {{read of variable whose lifetime has ended}}
}

constexpr int invalid = scope_escape(); // expected-error {{constexpr variable 'invalid' must be initialized by a constant expression}} \
                                            // expected-note {{in call to 'scope_escape()'}}
