// RUN: %clang_cc1 -triple arm64-apple-ios -fptrauth-intrinsics -fsyntax-only -verify %s

#include <ptrauth.h>

#define DISC0 100
#define DISC1 101

struct __attribute__((ptrauth_struct(2, DISC0))) SignedBase0 {};
static_assert(__builtin_ptrauth_struct_key(SignedBase0) == 2);
static_assert(__builtin_ptrauth_struct_disc(SignedBase0) == DISC0);

struct __attribute__((ptrauth_struct(2, DISC1))) SignedBase1 {};
struct UnsignedBase0 {};
struct __attribute__((ptrauth_struct(2, DISC0))) SignedDerivded0 : SignedBase0 {};
struct __attribute__((ptrauth_struct(2, DISC1))) SignedDerivded1 : SignedBase0 {};
struct __attribute__((ptrauth_struct(2, DISC0))) SignedDerivded2 : SignedBase0, SignedBase1 {};

struct __attribute__((ptrauth_struct(2, DISC0))) SignedDynamic0 { // expected-error {{cannot be used on class 'SignedDynamic0' or its subclasses because it is a dynamic class}}
  virtual void m0();
};
struct __attribute__((ptrauth_struct(2, DISC1))) SignedDynamic1 : virtual SignedBase1 {}; // expected-error {{cannot be used on class 'SignedDynamic1' or its subclasses because it is a dynamic class}}
struct UnsignedDynamic2 : virtual SignedBase0 {}; // expected-error {{because it is a dynamic class}}

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template0 : T {
  static_assert(__builtin_ptrauth_struct_key(Template0) == 2);
  static_assert(__builtin_ptrauth_struct_disc(Template0) == DISC0);
};

Template0<SignedBase0> g0;
Template0<SignedBase1> g1;
Template0<UnsignedBase0> g2;

struct DynamicBase0 {
  virtual void m0();
};

struct DynamicBase1 : virtual UnsignedBase0 {
};

struct DynamicBase2 : SignedBase0 { // expected-error {{because it is a dynamic class}}
  virtual void m0();
};

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template1 { // expected-error {{because it is a dynamic class}}
  virtual void m0();
};

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template2 : DynamicBase0 {}; // expected-error {{because it is a dynamic class}}

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template3 : DynamicBase1 {}; // expected-error {{because it is a dynamic class}}

template <class T>
struct __attribute__((ptrauth_struct(2, DISC1))) Template4 {
};

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template5 : Template4<T> {};

template <class T>
struct __attribute__((ptrauth_struct(2, DISC0))) Template6 : Template4<T> {};

template <>
struct __attribute__((ptrauth_struct(2, DISC0))) Template4<int> {
};

template <int k, int d>
struct __attribute__((ptrauth_struct(k, d))) Template7 { // expected-error 3 {{because it is a dynamic class}}
  virtual void m0();
};

template <int d>
struct Template8 : Template7<2, d> { // expected-error {{because it is a dynamic class}} expected-note {{in instantiation of template class}}
};

template <int d>
struct Template9 : Template7<2, 100> { // expected-error {{because it is a dynamic class}} expected-note {{in instantiation of template class}}
};

Template5<float> g3;
Template5<int> g4;
Template7<3, 100> g5; // expected-note {{in instantiation of template class}}
Template8<103> g6; // expected-note 2 {{in instantiation of template class}}
SignedBase0 * __ptrauth(ptrauth_key_none, 0, 0) g7; // expected-error {{signed pointer types may not be qualified}}
Template7<ptrauth_key_none, 0> * __ptrauth(ptrauth_key_none, 0, 0) g8;
