// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr unsigned f() {
  return 100;
}

static_assert(f() == 100);

static_assert((long long)(unsigned short)-1 == 65535);
static_assert((long long)(unsigned char)-1 == 255);
static_assert((long long)(short)-1 == -1);
static_assert((long long)(char)-1 == -1);
