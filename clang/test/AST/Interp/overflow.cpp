// RUN: %clang_cc1 -std=c++17 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++17 -fsyntax-only %s -verify

constexpr int add_pos(int a) {
  return a + a; // expected-note {{value 4000000000 is outside the range of representable values of type 'int'}}
}

constexpr int v0 = add_pos(2000000000); // expected-error {{constexpr variable 'v0' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_pos(2000000000)'}}

constexpr int add_neg(int a) {
  return a + a; // expected-note {{value -4000000000 is outside the range of representable values of type 'int'}}
}
constexpr int v1 = add_neg(-2000000000); // expected-error {{constexpr variable 'v1' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'add_neg(-2000000000)'}}

constexpr int sub_neg(int a, int b) {
  return a - b; // expected-note {{value -4000000000 is outside the range of representable values of type 'int'}}
}

constexpr int v2 = sub_neg(-2000000000, 2000000000); // expected-error {{constexpr variable 'v2' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_neg(-2000000000, 2000000000)'}}

constexpr int sub_pos(int a, int b) {
  return a - b; // expected-note {{value 4000000000 is outside the range of representable values of type 'int'}}
}
constexpr int v3 = sub_pos(2000000000, -2000000000); // expected-error {{constexpr variable 'v3' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'sub_pos(2000000000, -2000000000)'}}

constexpr int mul(int a, int b) {
  return a * b; // expected-note {{value 4900000000 is outside the range of representable values of type 'int'}}
}
constexpr int v4 = mul(70000, 70000); // expected-error {{constexpr variable 'v4' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mul(70000, 70000)'}}

constexpr int div_0(int a, int b) {
  return a / b; // expected-note {{division by zero}}
}
constexpr int v5 = div_0(1, 0); // expected-error {{constexpr variable 'v5' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_0(1, 0)'}}

constexpr int mod_0(int a, int b) {
  return a % b; // expected-note {{division by zero}}
}
constexpr int v6 = mod_0(1, 0); // expected-error {{constexpr variable 'v6' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_0(1, 0)'}}

constexpr int div_ov(int a, int b) {
  return a / b; // expected-note {{value 2147483648 is outside the range of representable values of type 'int'}}
}
constexpr int v7 = div_ov(-2147483647 - 1, -1); // expected-error {{constexpr variable 'v7' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'div_ov(-2147483648, -1)'}}
constexpr int mod_ov(int a, int b) {
  return a / b; // expected-note {{value 2147483648 is outside the range of representable values of type 'int'}}
}
constexpr int v8 = mod_ov(-2147483647 - 1, -1); // expected-error {{constexpr variable 'v8' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'mod_ov(-2147483648, -1)'}}

constexpr unsigned sub_u(unsigned a) {
  return 0u - a;
}
static_assert(sub_u(2000000000u) == 2294967296u);

constexpr int neg_ov(int a) {
  return -a; // expected-note {{value 2147483648 is outside the range of representable values of type 'int'}}
}
constexpr int v9 = neg_ov(-2147483647 - 1); // expected-error {{constexpr variable 'v9' must be initialized by a constant expression}} \
                                                        // expected-note {{in call to 'neg_ov(-2147483648)'}}

constexpr unsigned neg_ov_u(unsigned a) {
  return -a;
}
constexpr unsigned v10 = neg_ov_u(2147483647);
