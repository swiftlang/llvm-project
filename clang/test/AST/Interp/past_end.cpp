// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int add_pointer(int *st, int *en) {
  int sum = 0;
  for (int *i = st; i < en; i = i + 1) {
    sum = sum + i[0];
  }
  return sum;
}

constexpr int pointer_adder() {
  int arr[3] = {1, 2, 3};
  return add_pointer(arr, arr + 3);
}

using A = int[6];
using A = int[pointer_adder()];
