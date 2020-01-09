// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int cond_then_else(int a, int b) {
  if (a < b) {
    return b - a;
  } else {
    return a - b;
  }
}

static_assert(cond_then_else(10, 20) == 10);
static_assert(cond_then_else(30, 10) == 20);

constexpr int cond_empty_then(int a) {
  if (a < 0)
    ;
  return 1;
}

static_assert(cond_empty_then(0) == 1);

constexpr int cond_else(int a) {
  if (a < 0)
    return 10;
  return 1;
}

static_assert(cond_else(-1) == 10);
static_assert(cond_else(1) == 1);

constexpr int cond_decl(int a) {
  if (int b = a + 1)
    return b - 1;
  else
    return b + 1;
}

static_assert(cond_decl(-1) == 1);
static_assert(cond_decl(5) == 5);

constexpr int cond_decl_init(int a) {
  if (int b = a + 1; b != 0)
    return b - 1;
  else
    return b + 1;
}

static_assert(cond_decl_init(-1) == 1);
static_assert(cond_decl_init(5) == 5);
