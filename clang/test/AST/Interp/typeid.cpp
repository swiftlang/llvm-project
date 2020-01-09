// RUN: %clang_cc1 -std=c++2a -fcxx-exceptions -fsyntax-only -fexperimental-new-constant-interpreter %s -verify
// RUN: %clang_cc1 -std=c++2a -fcxx-exceptions -fsyntax-only %s -verify

namespace std {
  struct type_info;
}

struct A {
  const std::type_info &ti = typeid(*this);
};
struct A2 : A {};
static_assert(&A().ti == &typeid(A));
static_assert(&typeid((A2())) == &typeid(A2));
extern A2 extern_a2;
static_assert(&typeid(extern_a2) == &typeid(A2));

constexpr A2 a2;
constexpr const A &a1 = a2;
static_assert(&typeid(a1) == &typeid(A));

struct B {
  virtual void f();
  const std::type_info &ti1 = typeid(*this);
};
struct B2 : B {
  const std::type_info &ti2 = typeid(*this);
};
static_assert(&B2().ti1 == &typeid(B));
static_assert(&B2().ti2 == &typeid(B2));
extern B2 extern_b2;
// expected-note@+1 {{typeid applied to object 'extern_b2' whose dynamic type is not constant}}
static_assert(&typeid(extern_b2) == &typeid(B2)); // expected-error {{constant expression}}

constexpr B2 b2;
constexpr const B &b1 = b2;
static_assert(&typeid(b1) == &typeid(B2));

constexpr bool side_effects() {
  // Not polymorphic nor a glvalue.
  bool OK = true;
  (void)typeid(OK = false, A2()); // expected-warning {{has no effect}}
  if (!OK) return false;

  // Not polymorphic.
  A2 a2;
  (void)typeid(OK = false, a2); // expected-warning {{has no effect}}
  if (!OK) return false;

  // Not a glvalue.
  (void)typeid(OK = false, B2()); // expected-warning {{has no effect}}
  if (!OK) return false;

  // Polymorphic glvalue: operand evaluated.
  OK = false;
  B2 b2;
  (void)typeid(OK = true, b2); // expected-warning {{will be evaluated}}
  return OK;
}
static_assert(side_effects());
