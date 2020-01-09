// RUN: %clang_cc1 -triple=x86_64-unknown-linux -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -triple=x86_64-unknown-linux -std=c++17 -fsyntax-only %s -verify

constexpr long long MAX_VALUE = 0x7FFFFFFFFFFFFFFF;
constexpr long long MIN_VALUE = -MAX_VALUE - 1;
constexpr long long add_pos(long long a) {
  return a + a;                                         // expected-note {{value 18446744073709551614 is outside the range of representable values of type 'long long'}}
}

constexpr long long v0 = add_pos(MAX_VALUE);             // expected-error {{constexpr variable 'v0' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_pos(9223372036854775807)'}}

constexpr long long add_neg(long long a) {
  return a + a;                                         // expected-note {{value -18446744073709551616 is outside the range of representable values of type 'long long'}}
}
constexpr long long v1 = add_neg(MIN_VALUE);             // expected-error {{constexpr variable 'v1' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_neg(-9223372036854775808)'}}

constexpr long long sub_neg(long long a, long long b) {
  return a - b;                                         // expected-note {{value -18446744073709551615 is outside the range of representable values of type 'long long'}}
}

constexpr long long v2 = sub_neg(MIN_VALUE, MAX_VALUE);  // expected-error {{constexpr variable 'v2' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_neg(-9223372036854775808, 9223372036854775807)'}}

constexpr long long sub_pos(long long a, long long b) {
  return a - b;                                         // expected-note {{value 18446744073709551615 is outside the range of representable values of type 'long long'}}
}
constexpr long long v3 = sub_pos(MAX_VALUE, MIN_VALUE);  // expected-error {{constexpr variable 'v3' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_pos(9223372036854775807, -9223372036854775808)'}}

constexpr long long mul(long long a, long long b) {
  return a * b;                                         // expected-note {{value 85070591730234615847396907784232501249 is outside the range of representable values of type 'long long'}}
}
constexpr long long v4 = mul(MAX_VALUE, MAX_VALUE);      // expected-error {{constexpr variable 'v4' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mul(9223372036854775807, 9223372036854775807)'}}

constexpr long long div_0(long long a, long long b) {
  return a / b;                                         // expected-note {{division by zero}}
}
constexpr long long v5 = div_0(1, 0);                    // expected-error {{constexpr variable 'v5' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_0(1, 0)'}}

constexpr long long mod_0(long long a, long long b) {
  return a % b;                                         // expected-note {{division by zero}}
}
constexpr long long v6 = mod_0(1, 0);                    // expected-error {{constexpr variable 'v6' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_0(1, 0)'}}

constexpr long long div_ov(long long a, long long b) {
  return a / b;                                         // expected-note {{value 9223372036854775808 is outside the range of representable values of type 'long long'}}
}
constexpr long long v7 = div_ov(MIN_VALUE, -1);          // expected-error {{constexpr variable 'v7' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_ov(-9223372036854775808, -1)'}}
constexpr long long mod_ov(long long a, long long b) {
  return a / b;                                         // expected-note {{value 9223372036854775808 is outside the range of representable values of type 'long long'}}
}
constexpr long long v8 = mod_ov(MIN_VALUE, -1);          // expected-error {{constexpr variable 'v8' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_ov(-9223372036854775808, -1)'}}

constexpr long long neg_ov(long long a) {
  return -a;                                            // expected-note {{value 9223372036854775808 is outside the range of representable values of type 'long long'}}
}
constexpr long long v9 = neg_ov(MIN_VALUE);              // expected-error {{constexpr variable 'v9' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'neg_ov(-9223372036854775808)'}}

constexpr long long sub_zero_min(long long a, long long b) {
  return a - b;                                         // expected-note {{value 9223372036854775808 is outside the range of representable values of type 'long long'}}
}

constexpr long long v10 = sub_zero_min(0, MIN_VALUE);  // expected-error {{constexpr variable 'v10' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_zero_min(0, -9223372036854775808)'}}


static_assert((long long)-1 == -1);
static_assert((long long)(unsigned char)-1 == 255);
