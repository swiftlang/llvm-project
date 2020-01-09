// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify

union USimple {
  int a;
  int b;
};

constexpr USimple x1 = {42};
constexpr USimple y1 = x1;
static_assert(y1.a == 42, "");
static_assert(y1.b == 42, ""); // expected-error {{static_assert expression is not an integral constant expression}}\
                               // expected-note {{read of member 'b' of union with active member 'a' is not allowed in a constant expression}}

struct S {
  int x;
  int y;
};

union UStruct {
  S s;
  int x;
};

constexpr UStruct x2 = {{1, 2}};
constexpr UStruct y2 = x2;

static_assert(y2.s.x == 1, "");
static_assert(y2.s.y == 2, "");
static_assert(y2.x, "");       // expected-error {{static_assert expression is not an integral constant expression}}\
                               // expected-note {{read of member 'x' of union with active member 's' is not allowed in a constant expression}}

union UArray {
  int a[10];
  int b;
};

constexpr UArray x3 = {{[1] = 3}}; // expected-warning {{array designators are a C99 extension}}
constexpr UArray y3 = x3;

static_assert(y3.a[0] == 0, "");
static_assert(y3.a[1] == 3, "");
static_assert(y3.b, "");       // expected-error {{static_assert expression is not an integral constant expression}}\
                               // expected-note {{read of member 'b' of union with active member 'a' is not allowed in a constant expression}}
