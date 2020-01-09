// RUN: %clang_cc1 -std=c++2a -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fsyntax-only %s -verify
// expected-no-diagnostics

class Base {
public:
  virtual constexpr int f() const = 0;
};

class A : public Base {
public:
  int x = 1;
  constexpr int f() const override { return x; }
};

class B : public Base {
public:
  int x = 2;
  virtual constexpr int f() const override { return x; }
};

class C : public B {
public:
  int x = 3;
  constexpr int f() const override { return x; }
};

class D : public B {
public:
  int x = 4;
  constexpr int f() const override { return x; }
};

static_assert(A().f() == 1);
static_assert(B().f() == 2);
static_assert(C().f() == 3);
static_assert(D().f() == 4);
constexpr int call(const Base &b) {
  return b.f();
}

static_assert(call(A{}) == 1);
static_assert(call(B{}) == 2);
static_assert(call(C{}) == 3);
static_assert(call(D{}) == 4);
