// RUN: %clang_cc1 -std=c++11 -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++11 -fsyntax-only %s -verify

struct A {
  constexpr int a() const { return 1; }
};

struct B : public A {
  constexpr int b() const { return 2; }
};

struct C : public B {
  constexpr int c() const { return 3; }
};

constexpr A a{};
constexpr B b{};
constexpr C c{};

// Test direct pointers.
constexpr int (A::*pAa)() const = &A::a;
static_assert((a.*pAa)() == 1, "");

constexpr int (B::*pBa)() const = &B::a;
constexpr int (B::*pBb)() const = &B::b;
static_assert((b.*pBa)() == 1, "");
static_assert((b.*pBb)() == 2, "");

constexpr int (C::*pCa)() const = &C::a;
constexpr int (C::*pCb)() const = &C::b;
constexpr int (C::*pCc)() const = &C::c;
static_assert((c.*pCa)() == 1, "");
static_assert((c.*pCb)() == 2, "");
static_assert((c.*pCc)() == 3, "");

// Cast from base to derived.
constexpr int (B::*pABa)() const = pAa;
static_assert((b.*pABa)() == 1, "");

constexpr int (C::*pACa)() const = pAa;
static_assert((c.*pACa)() == 1, "");

constexpr int (C::*pABCa)() const = pBa;
static_assert((c.*pABCa)() == 1, "");

constexpr int (C::*pBCa)() const = pBa;
static_assert((c.*pBCa)() == 1, "");

constexpr int (C::*pBCb)() const = pBb;
static_assert((c.*pBCb)() == 2, "");

// Cast from derived to base.
constexpr int (B::*pCBa)() const = (int(B::*)() const)pCa;
constexpr int (B::*pCBb)() const = (int(B::*)() const)pCb;
constexpr int (B::*pCBc)() const = (int(B::*)() const)pCc;

constexpr int (A::*pCAa)() const = (int(A::*)() const)pCa;
constexpr int (A::*pCAb)() const = (int(A::*)() const)pCb;
constexpr int (A::*pCAc)() const = (int(A::*)() const)pCc;

constexpr int (A::*pCBAa)() const = (int(A::*)() const)pCBa;
constexpr int (A::*pCBAb)() const = (int(A::*)() const)pCBb;
constexpr int (A::*pCBAc)() const = (int(A::*)() const)pCBc;

static_assert((c.*pCBa)() == 1, "");
static_assert((c.*pCBb)() == 2, "");
static_assert((c.*pCBc)() == 3, "");
static_assert((c.*pCAa)() == 1, "");
static_assert((c.*pCAb)() == 2, "");
static_assert((c.*pCAc)() == 3, "");
static_assert((c.*pCBAa)() == 1, "");
static_assert((c.*pCBAb)() == 2, "");
static_assert((c.*pCBAc)() == 3, "");

static_assert((b.*pCBa)() == 1, "");
static_assert((b.*pCBb)() == 2, "");
static_assert((b.*pCBc)() == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert((b.*pCAa)() == 1, "");
static_assert((b.*pCAb)() == 2, "");
static_assert((b.*pCAc)() == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert((b.*pCBAa)() == 1, "");
static_assert((b.*pCBAb)() == 2, "");
static_assert((b.*pCBAc)() == 3, ""); // expected-error {{static_assert expression is not an integral constant expression}}

static_assert((a.*pCAa)() == 1, "");
static_assert((a.*pCAb)() == 2, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert((a.*pCAc)() == 3, "");  // expected-error {{static_assert expression is not an integral constant expression}}
static_assert((a.*pCBAa)() == 1, "");
static_assert((a.*pCBAb)() == 2, ""); // expected-error {{static_assert expression is not an integral constant expression}}
static_assert((a.*pCBAc)() == 3, ""); // expected-error {{static_assert expression is not an integral constant expression}}

// Cast from B to A.
constexpr int (A::*qBAa)() const = (int(A::*)() const)pBa;
constexpr int (A::*qBAb)() const = (int(A::*)() const)pBb;

static_assert((c.*qBAa)() == 1, "");
static_assert((c.*qBAb)() == 2, "");
static_assert((b.*qBAa)() == 1, "");
static_assert((b.*qBAb)() == 2, "");
static_assert((a.*qBAa)() == 1, "");
static_assert((a.*qBAb)() == 2, ""); // expected-error {{static_assert expression is not an integral constant expression}}

// Cast from A to B and C.
constexpr int (B::*qABa)() const = (int(B::*)() const)pAa;
constexpr int (C::*qACa)() const = (int(C::*)() const)pAa;
constexpr int (C::*qABCa)() const = (int(C::*)() const)pAa;

static_assert((b.*qABa)() == 1, "");
static_assert((c.*qACa)() == 1, "");
static_assert((c.*qABCa)() == 1, "");

// Cast from B to C.
constexpr int (C::*qBCa)() const = (int(C::*)() const)pBa;
constexpr int (C::*qBCb)() const = (int(C::*)() const)pBb;

static_assert((c.*qBCa)() == 1, "");
static_assert((c.*qBCb)() == 2, "");
