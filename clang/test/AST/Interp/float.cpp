// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr float a = 16777217;     // expected-warning {{implicit conversion from 'int' to 'const float' changes value from 16777217 to 16777216}}

constexpr float f = 1.0 / 0.0f;   // expected-error {{constexpr variable 'f' must be initialized by a constant expression}}\
                                  // expected-note {{division by zero}}

static_assert(1 + 2.0f == 3.0f);
static_assert(4.0f + 1 == 5.0f);
static_assert(1.0f + 5.0f == 6.0f);
static_assert(1 + 1.0 == 2.0);
static_assert(1.0 + 2 == 3.0);
static_assert(1.0 + 1.0f == 2.0);
static_assert(1.0f + 1.0 == 2.0);
static_assert(1.0 + 1.0f == 2.0);


constexpr float inc_float() {
  float x = 3.0f;
  return ++x;
}
static_assert(inc_float() == 4.0f);

constexpr double inc_double() {
  double x = 3.0;
  return ++x;
}
static_assert(inc_double() == 4.0);

constexpr float dec_float() {
  float x = 3.0f;
  return --x;
}
static_assert(dec_float() == 2.0f);

constexpr double dec_double() {
  double x = 3.0;
  return --x;
}
static_assert(dec_double() == 2.0);
