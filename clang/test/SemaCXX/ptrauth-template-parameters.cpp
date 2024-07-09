// RUN: %clang_cc1 -triple arm64e-apple-ios -fsyntax-only -verify -fptrauth-intrinsics -std=c++20 %s

#if __has_feature(ptrauth_restricted_intptr_qualifier)

template <typename T> struct S {
  T __ptrauth_restricted_intptr(0,0,1234) test;
  // expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'void *'}}
  // expected-error@-2{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'int'}}
  // expected-error@-3 3 {{type '__ptrauth_restricted_intptr(0,0,1234) T' is already __ptrauth_restricted_intptr-qualified}}
};

void f1() {
  S<__INTPTR_TYPE__> basic;
  S<int> invalid_type;
  // expected-note@-1{{in instantiation of template class 'S<int>' requested here}}
  S<void *> mismatched_pointer_type;
  // expected-note@-1{{in instantiation of template class 'S<void *>' requested here}}
  S<void *__ptrauth_restricted_intptr(0,0,1234)> mismatched_pointer_type_incorrect_ptrauth1;
  // expected-error@-1 {{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'void *'}}
  S<void *__ptrauth(0,0,1234)> mismatched_pointer_type_correct_ptrauth;
  // expected-note@-1{{in instantiation of template class 'S<void *__ptrauth(0,0,1234)>' requested here}}
  S<__INTPTR_TYPE__ __ptrauth_restricted_intptr(0,0,1234)> matched;
  // expected-note@-1{{in instantiation of template class 'S<__ptrauth_restricted_intptr(0,0,1234) long>'}}
  S<__INTPTR_TYPE__ __ptrauth_restricted_intptr(0,0,1235)> mismatching_qualifier1;
  // expected-note@-1{{in instantiation of template class 'S<__ptrauth_restricted_intptr(0,0,1235) long>' requested here}}
  S<__INTPTR_TYPE__ __ptrauth(0,0,1234)> mismatching_qualifier2;
  // expected-error@-1{{__ptrauth qualifier may only be applied to pointer types; type here is 'long'}}
  S<__INTPTR_TYPE__ __ptrauth(0,0,1235)> mismatching_qualifier3;
  // expected-error@-1{{__ptrauth qualifier may only be applied to pointer types; type here is 'long'}}
};

void f2() {
  S<__INTPTR_TYPE__> unqualified;
  S<__INTPTR_TYPE__ __ptrauth_restricted_intptr(0,0,1234)> qualified;
  __INTPTR_TYPE__ __ptrauth_restricted_intptr(0,0,1234)* p;
  p = &unqualified.test;
  p = &qualified.test;
  __INTPTR_TYPE__ *mismatch;
  mismatch = &unqualified.test;
  // expected-error@-1{{assigning '__ptrauth_restricted_intptr(0,0,1234) long *' to 'long *' changes pointer-authentication of pointee type}}
  mismatch = &qualified.test;
}

template <typename T> struct G {
  T __ptrauth(0,0,1234) test;
  // expected-error@-1 3 {{type '__ptrauth(0,0,1234) T' is already __ptrauth-qualified}}
};

template <typename T> struct Indirect {
  G<T> layers;
  // expected-note@-1{{in instantiation of template class 'G<void *__ptrauth(0,0,1235)>' requested here}}
  // expected-note@-2{{in instantiation of template class 'G<void *__ptrauth(ptrauth_key_none,0,1235)>' requested here}}
  // expected-note@-3{{in instantiation of template class 'G<void *__ptrauth(0,0,1234)>' requested here}}
};

void f3() {
  Indirect<void* __ptrauth(0,0,1234)> one;
  // expected-note@-1{{in instantiation of template class 'Indirect<void *__ptrauth(0,0,1234)>' requested here}}
  Indirect<void* __ptrauth(0,0,1235)> two;
  // expected-note@-1{{in instantiation of template class 'Indirect<void *__ptrauth(0,0,1235)>' requested here}}
  Indirect<void*> three;
  Indirect<void* __ptrauth(-1,0,1235)> four;
  // expected-note@-1{{in instantiation of template class 'Indirect<void *__ptrauth(ptrauth_key_none,0,1235)>' requested here}}
}


template <typename P> struct __attribute__((ptrauth_struct(0,1236))) AuthenticatedStruct  {
  P ptr;
};

void f4(void* __attribute__((nonnull)) v) {
  AuthenticatedStruct<void*> no_ptrauth;
  AuthenticatedStruct<void* __ptrauth(0,0,1237)> basic_auth;
  AuthenticatedStruct<void* __ptrauth(-1,0,1238)> explicit_null_auth;
  no_ptrauth.ptr=v;
  basic_auth.ptr=v;
  explicit_null_auth.ptr=v;
}

#endif
