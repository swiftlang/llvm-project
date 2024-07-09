// RUN: %clang_cc1  -x c++ -std=c++11 -triple arm64-apple-ios -verify -fptrauth-intrinsics %s

typedef void *__ptrauth(2, 1, 0, "authenticates-null-values") authenticated_null_ptr;
typedef unsigned long long __ptrauth_restricted_intptr(2, 1, 0, "authenticates-null-values") authenticated_null_uintptr;
typedef void *__ptrauth(2, 1, 0) unauthenticated_null_ptr;
typedef unsigned long long __ptrauth_restricted_intptr(2, 1, 0) unauthenticated_null_uintptr;

struct S {
  authenticated_null_ptr a;
};

struct S s = {0};
// expected-error@-1 {{globals with authenticated null values are currently unsupported}}

authenticated_null_ptr a;
// expected-error@-1 {{globals with authenticated null values are currently unsupported}}
authenticated_null_uintptr b;
// expected-error@-1 {{globals with authenticated null values are currently unsupported}}

unauthenticated_null_ptr c;
unauthenticated_null_uintptr d;

int func1() {
  static authenticated_null_ptr auth;
  // expected-error@-1 {{static locals with authenticated null values are currently unsupported}}
  return *((int *)auth);
}

int func3() {
  static authenticated_null_uintptr auth = 0;
  // expected-error@-1 {{static locals with authenticated null values are currently unsupported}}
  return auth++;
}

static authenticated_null_uintptr auth_int;
// expected-error@-1 {{globals with authenticated null values are currently unsupported}}
static authenticated_null_ptr auth;
// expected-error@-1 {{globals with authenticated null values are currently unsupported}}
int func4() {
  static S auth;
  // expected-error@-1 {{static locals with authenticated null values are currently unsupported}}
  return (int)(unsigned long long)auth.a;
}
