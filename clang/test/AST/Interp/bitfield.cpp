// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

struct S128 {
  __int128 n : 3;
  constexpr S128(__int128 k) : n(k) {}
};
static_assert(S128(3).n == 3, "");
static_assert(S128(4).n == -4, "");
static_assert(S128(7).n == -1, "");
static_assert(S128(8).n == 0, "");
static_assert(S128(-1).n == -1, "");
static_assert(S128(-8889).n == -1, "");

struct S64 {
  long long n : 3;
  constexpr S64(long long k) : n(k) {}
};
static_assert(S64(3).n == 3, "");
static_assert(S64(4).n == -4, "");
static_assert(S64(7).n == -1, "");
static_assert(S64(8).n == 0, "");
static_assert(S64(-1).n == -1, "");
static_assert(S64(-8889).n == -1, "");



constexpr int shift_bitfield() {
  S64 a{1};
  a.n <<= 1;
  return a.n;
}
static_assert(shift_bitfield() == 2);


constexpr int shift_bitfield_trunc() {
  S64 a{1};
  a.n <<= 10;
  return a.n;
}
static_assert(shift_bitfield_trunc() == 0);
