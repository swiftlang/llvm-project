// RUN: %clang_cc1 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++11 -fsyntax-only %s -verify

struct A {
  int a = 1;
};

struct B : public A {
  int b = 2;
};

struct C : public B {
  int c = 3;
};

constexpr A a{};
constexpr B b{};
constexpr C c{};

// Test direct pointers.
constexpr int (A::*pAa) = &A::a;
static_assert(a.*pAa == 1, "");

constexpr int (B::*pBa) = &B::a;
constexpr int (B::*pBb) = &B::b;
static_assert(b.*pBa == 1, "");
static_assert(b.*pBb == 2, "");

constexpr int (C::*pCa) = &C::a;
constexpr int (C::*pCb) = &C::b;
constexpr int (C::*pCc) = &C::c;
static_assert(c.*pCa == 1, "");
static_assert(c.*pCb == 2, "");
static_assert(c.*pCc == 3, "");

// Cast from base to derived.
constexpr int (B::*pABa) = pAa;
static_assert(b.*pABa == 1, "");

constexpr int (C::*pACa) = pAa;
static_assert(c.*pACa == 1, "");

constexpr int (C::*pABCa) = pBa;
static_assert(c.*pABCa == 1, "");

constexpr int (C::*pBCa) = pBa;
static_assert(c.*pBCa == 1, "");

constexpr int (C::*pBCb) = pBb;
static_assert(c.*pBCb == 2, "");

// Cast from C to B and A.
constexpr int (B::*pCBa) = (int(B::*))pCa;
constexpr int (B::*pCBb) = (int(B::*))pCb;
constexpr int (B::*pCBc) = (int(B::*))pCc;

constexpr int (A::*pCAa) = (int(A::*))pCa;
constexpr int (A::*pCAb) = (int(A::*))pCb;
constexpr int (A::*pCAc) = (int(A::*))pCc;

constexpr int (A::*pCBAa) = (int(A::*))pCBa;
constexpr int (A::*pCBAb) = (int(A::*))pCBb;
constexpr int (A::*pCBAc) = (int(A::*))pCBc;

static_assert(c.*pCBa == 1, "");
static_assert(c.*pCBb == 2, "");
static_assert(c.*pCBc == 3, "");
static_assert(c.*pCAa == 1, "");
static_assert(c.*pCAb == 2, "");
static_assert(c.*pCAc == 3, "");
static_assert(c.*pCBAa == 1, "");
static_assert(c.*pCBAb == 2, "");
static_assert(c.*pCBAc == 3, "");

static_assert(b.*pCBa == 1, "");
static_assert(b.*pCBb == 2, "");
static_assert(b.*pCBc == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert(b.*pCAa == 1, "");
static_assert(b.*pCAb == 2, "");
static_assert(b.*pCAc == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert(b.*pCBAa == 1, "");
static_assert(b.*pCBAb == 2, "");
static_assert(b.*pCBAc == 3, ""); // expected-error {{static_assert expression is not an integral constant expression}}

static_assert(a.*pCAa == 1, "");
static_assert(a.*pCAb == 2, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert(a.*pCAc == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert(a.*pCBAa == 1, "");
static_assert(a.*pCBAb == 2, ""); // expected-error {{static_assert expression is not an integral constant expression}}
static_assert(a.*pCBAc == 3, ""); // expected-error {{static_assert expression is not an integral constant expression}}

// Cast from B to A.
constexpr int (A::*qBAa) = (int(A::*))pBa;
constexpr int (A::*qBAb) = (int(A::*))pBb;

static_assert(c.*qBAa == 1, "");
static_assert(c.*qBAb == 2, "");
static_assert(b.*qBAa == 1, "");
static_assert(b.*qBAb == 2, "");
static_assert(a.*qBAa == 1, "");
static_assert(a.*qBAb == 2, ""); // expected-error {{static_assert expression is not an integral constant expression}}

// Cast from A to B and C.
constexpr int (B::*qABa) = (int(B::*))pAa;
constexpr int (C::*qACa) = (int(C::*))pAa;
constexpr int (C::*qABCa) = (int(C::*))pAa;

static_assert(b.*qABa == 1, "");
static_assert(c.*qACa == 1, "");
static_assert(c.*qABCa == 1, "");

// Cast from B to C.
constexpr int (C::*qBCa) = (int(C::*))pBa;
constexpr int (C::*qBCb) = (int(C::*))pBb;

static_assert(c.*qBCa == 1, "");
static_assert(c.*qBCb == 2, "");
