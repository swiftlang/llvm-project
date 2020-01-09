// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr __int128 MAX_VALUE = (static_cast<__int128>(0x7FFFFFFFFFFFFFFF) << static_cast<__int128>(64)) | static_cast<__int128>(0xFFFFFFFFFFFFFFFF);
constexpr __int128 MIN_VALUE = -MAX_VALUE - 1;
constexpr __int128 add_pos(__int128 a) {
  return a + a;                                         // expected-note {{value 340282366920938463463374607431768211454 is outside the range of representable values of type '__int128'}}
}

constexpr __int128 v0 = add_pos(MAX_VALUE);             // expected-error {{constexpr variable 'v0' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_pos(170141183460469231731687303715884105727)'}}

constexpr __int128 add_neg(__int128 a) {
  return a + a;                                         // expected-note {{value -340282366920938463463374607431768211456 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v1 = add_neg(MIN_VALUE);             // expected-error {{constexpr variable 'v1' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_neg(-170141183460469231731687303715884105728)'}}

constexpr __int128 sub_neg(__int128 a, __int128 b) {
  return a - b;                                         // expected-note {{value -340282366920938463463374607431768211455 is outside the range of representable values of type '__int128'}}
}

constexpr __int128 v2 = sub_neg(MIN_VALUE, MAX_VALUE);  // expected-error {{constexpr variable 'v2' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_neg(-170141183460469231731687303715884105728, 170141183460469231731687303715884105727)'}}

constexpr __int128 sub_pos(__int128 a, __int128 b) {
  return a - b;                                         // expected-note {{value 340282366920938463463374607431768211455 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v3 = sub_pos(MAX_VALUE, MIN_VALUE);  // expected-error {{constexpr variable 'v3' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_pos(170141183460469231731687303715884105727, -170141183460469231731687303715884105728)'}}

constexpr __int128 mul(__int128 a, __int128 b) {
  return a * b;                                         // expected-note {{value 28948022309329048855892746252171976962977213799489202546401021394546514198529 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v4 = mul(MAX_VALUE, MAX_VALUE);      // expected-error {{constexpr variable 'v4' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mul(170141183460469231731687303715884105727, 170141183460469231731687303715884105727)'}}

constexpr __int128 div_0(__int128 a, __int128 b) {
  return a / b;                                         // expected-note {{division by zero}}
}
constexpr __int128 v5 = div_0(1, 0);                    // expected-error {{constexpr variable 'v5' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_0(1, 0)'}}

constexpr __int128 mod_0(__int128 a, __int128 b) {
  return a % b;                                         // expected-note {{division by zero}}
}
constexpr __int128 v6 = mod_0(1, 0);                    // expected-error {{constexpr variable 'v6' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_0(1, 0)'}}

constexpr __int128 div_ov(__int128 a, __int128 b) {
  return a / b;                                         // expected-note {{value 170141183460469231731687303715884105728 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v7 = div_ov(MIN_VALUE, -1);          // expected-error {{constexpr variable 'v7' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_ov(-170141183460469231731687303715884105728, -1)'}}
constexpr __int128 mod_ov(__int128 a, __int128 b) {
  return a / b;                                         // expected-note {{value 170141183460469231731687303715884105728 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v8 = mod_ov(MIN_VALUE, -1);          // expected-error {{constexpr variable 'v8' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_ov(-170141183460469231731687303715884105728, -1)'}}

constexpr __int128 neg_ov(__int128 a) {
  return -a;                                            // expected-note {{value 170141183460469231731687303715884105728 is outside the range of representable values of type '__int128'}}
}
constexpr __int128 v9 = neg_ov(MIN_VALUE);              // expected-error {{constexpr variable 'v9' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'neg_ov(-170141183460469231731687303715884105728)'}}

static_assert((__int128)-1 == -1);
static_assert((__int128)(unsigned short)-1 == 65535);
