// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int error(void *p, int arg) {
    return 1 / arg; // expected-note {{division by zero}}
}

int main() {
  // expected-error@+2 {{static_assert expression is not an integral constant expression}}
  // expected-note@+1 {{in call to 'error(&&&x, 0)'}}
  x: static_assert(error(&&x, 0), "");

  y: static_assert(&&y, "");
}
