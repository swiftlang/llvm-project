// RUN: %clang_cc1 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter -verify=all,cpp11 %s
// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter -verify=all %s
// RUN: %clang_cc1 -std=c++11 -fsyntax-only -verify=all,cpp11 %s
// RUN: %clang_cc1 -std=c++2a -fsyntax-only -verify=all %s

constexpr int shls(int a, int b) {
  return a << b;                  // all-note {{negative shift count -200}}\
                                  // all-note {{shift count 200 >= width of type 'int' (32 bits)}}\
                                  // cpp11-note {{left shift of negative value -1}}\
                                  // cpp11-note {{signed left shift discards bits}}
}

constexpr int v1 = shls(1, -200); // all-error {{constexpr variable 'v1' must be initialized by a constant expression}}\
                                  // all-note {{in call to 'shls(1, -200)'}}

constexpr int v0 = shls(1, 200);  // all-error {{constexpr variable 'v0' must be initialized by a constant expression}}\
                                  // all-note {{in call to 'shls(1, 200)'}}

using A0 = int[8];
using A0 = int[shls(4, 1)];
using A1 = int[2];
using A1 = int[shls(4, -1)];

constexpr int v4 = shls(-1, 1);   // cpp11-error {{constexpr variable 'v4' must be initialized by a constant expression}}\
                                  // cpp11-note {{in call to 'shls(-1, 1)'}}

constexpr int v5 = shls(128, 30); // cpp11-error {{constexpr variable 'v5' must be initialized by a constant expression}}\
                                  // cpp11-note {{in call to 'shls(128, 30)'}}


constexpr int shrs(int a, int b) {
  return a >> b;                  // all-note {{negative shift count -200}}\
                                  // all-note {{shift count 200 >= width of type 'int' (32 bits)}}\
                                  // all-note {{shift count 32 >= width of type 'int' (32 bits)}}
}

constexpr int v2 = shrs(1, -200); // all-error {{constexpr variable 'v2' must be initialized by a constant expression}}\
                                  // all-note {{in call to 'shrs(1, -200)'}}

constexpr int v3 = shrs(1, 200);  // all-error {{constexpr variable 'v3' must be initialized by a constant expression}}\
                                  // all-note {{in call to 'shrs(1, 200)'}}

using B0 = int[2];
using B0 = int[shrs(4, 1)];
using B1 = int[8];
using B1 = int[shrs(4, -1)];

constexpr int v6 = shrs(1, 32);   // all-error {{constexpr variable 'v6' must be initialized by a constant expression}}\
                                  // all-note {{in call to 'shrs(1, 32)'}}

static_assert(-1 >> 1 == -1, "");
static_assert(-1 >> 31 == -1, "");
static_assert(-2 >> 1 == -1, "");
static_assert(-3 >> 1 == -2, "");
static_assert(-4 >> 1 == -2, "");
