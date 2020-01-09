// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr bool for_range_init() {
  int k = 0;
  for (int arr[3] = {1, 2, 3}; int n : arr) k += n;
  return k == 6;
}
static_assert(for_range_init());


constexpr int for_range_break() {
  int k = 0;
  int arr[5] = { 0, 0, 0, 1, 3};
  for (int n : arr) {
    if (n == 1) {
      break;
    }
    ++k;
  }
  return k;
}
using A = int[3];
using A = int[for_range_break()];

constexpr int for_range_continue() {
  int k = 0;
  int arr[5] = { 0, 0, 0, 1, 0};
  for (int n : arr) {
    if (n == 0) {
      continue;
    }
    ++k;
  }
  return k;
}
using B = int[1];
using B = int[for_range_continue()];
