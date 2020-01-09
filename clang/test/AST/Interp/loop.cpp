// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int sum_pow(int n) {
  int a = 0;
  for (int i = 0; i < n; i = i + 1) {
    for (int j = 0; j < n; j = j + 1) {
      a = (a + (i % 1000) + (j % 1000)) % 1000;
    }
  }
  return a;
}

static_assert(sum_pow(2) == 4);

constexpr int loop_cond_var(int n) {
  int a = 0;
  for (int i = n; int b = a + 1; i = i + 1) {
    if (i == 50) {
      a = 0 - b;
    }
  }
  return a - 1;
}

static_assert(loop_cond_var(10) == -2);
