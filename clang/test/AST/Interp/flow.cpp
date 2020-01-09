// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int fn_for_break(int n) {
  int x = 0;
  for (int i = 0; i < 20; ++i) {
    if (i == n)
      break;
    x++;
  }
  return x;
}
using A = int[5];
using A = int[fn_for_break(5)];

constexpr int fn_for_cont(int n) {
  int x = 0;
  for (int i = 0; i < 20; ++i) {
    if (i < n)
      continue;
    x++;
  }
  return x;
}
using B = int[15];
using B = int[fn_for_cont(5)];

constexpr int fn_while_break(unsigned n) {
  int x = 0;
  int i = 0;
  while (int next = ++i) {
    if (next == n) {
      break;
    }
    x++;
  }
  return x;
}
using C = int[4];
using C = int[fn_while_break(5)];

constexpr int fn_while_cont(unsigned n) {
  int x = 0;
  int i = 0;
  while (i < 10) {
    ++i;
    if (i < n) {
      continue;
    }
    x++;
  }
  return x;
}
using D = int[6];
using D = int[fn_while_cont(5)];

constexpr int fn_do_break(unsigned n) {
  int x = 0;
  int i = 0;
  do {
    if (i == n) {
      break;
    }
    x++;
    i++;
  } while (i < 20);
  return x;
}
using E = int[5];
using E = int[fn_do_break(5)];

constexpr int fn_do_continue(unsigned n) {
  int x = 0;
  int i = 0;
  do {
    i++;
    if (i < n) {
      continue;
    }
    x++;
  } while (i < 20);
  return x;
}
using F = int[16];
using F = int[fn_do_continue(5)];
