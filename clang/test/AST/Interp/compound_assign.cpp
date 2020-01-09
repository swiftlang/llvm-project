// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify
// expected-no-diagnostics

constexpr int MulAssign(int a, int b) {
  int x = a;
  return x *= b;
}
using ResultMulAssign = int[6];
using ResultMulAssign = int[MulAssign(3, 2)];

constexpr int DivAssign(int a, int b) {
  int x = a;
  return x /= b;
}
using ResultDivAssign = int[3];
using ResultDivAssign = int[DivAssign(6, 2)];

constexpr int RemAssign(int a, int b) {
  int x = a;
  return x %= b;
}
using ResultRemAssign = int[1];
using ResultRemAssign = int[RemAssign(5, 2)];

constexpr int AddAssign(int a, int b) {
  int x = a;
  return x += b;
}
using ResultAddAssign = int[3];
using ResultAddAssign = int[AddAssign(1, 2)];

constexpr int SubAssign(int a, int b) {
  int x = a;
  return x -= b;
}
using ResultSubAssign = int[3];
using ResultSubAssign = int[SubAssign(5, 2)];

constexpr int AndAssign(int a, int b) {
  int x = a;
  return x &= b;
}
using ResultAndAssign = int[4];
using ResultAndAssign = int[AndAssign(7, 4)];

constexpr int XorAssign(int a, int b) {
  int x = a;
  return x ^= b;
}
using ResultXorAssign = int[1];
using ResultXorAssign = int[XorAssign(5, 4)];

constexpr int OrAssign(int a, int b) {
  int x = a;
  return x |= b;
}
using ResultOrAssign = int[6];
using ResultOrAssign = int[OrAssign(4, 2)];

constexpr int ShlAssign(int a, int b) {
  int x = a;
  return x <<= b;
}
static_assert(ShlAssign(1, 2) == 4);

constexpr int ShrAssign(int a, int b) {
  int x = a;
  return x >>= b;
}
static_assert(ShrAssign(4, 1) == 2);
