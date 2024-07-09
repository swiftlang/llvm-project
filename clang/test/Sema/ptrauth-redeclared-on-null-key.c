// RUN: %clang_cc1 -triple arm64-apple-ios -verify -fptrauth-intrinsics %s

#include <ptrauth.h>

typedef void* __ptrauth(1,1,1) p1;
typedef p1  __ptrauth(ptrauth_key_none,1,1) p2;
// expected-error@-1 {{type 'p1' (aka 'void *__ptrauth(1,1,1)') is already __ptrauth-qualified}}

typedef __INTPTR_TYPE__ intptr_t;
typedef intptr_t __ptrauth_restricted_intptr(1,1,1) i1;
typedef i1 __ptrauth_restricted_intptr(ptrauth_key_none,1,1) i2;
// expected-error@-1 {{type 'i1' (aka '__ptrauth_restricted_intptr(1,1,1) long') is already __ptrauth_restricted_intptr-qualified}}

typedef void* __ptrauth(ptrauth_key_none,1,1) p3;
typedef p1  __ptrauth(1,1,1) p4;
// expected-error@-1 {{type 'p1' (aka 'void *__ptrauth(1,1,1)') is already __ptrauth-qualified}}

typedef __INTPTR_TYPE__ intptr_t;
typedef intptr_t __ptrauth_restricted_intptr(ptrauth_key_none,1,1) i3;
typedef i1 __ptrauth_restricted_intptr(1,1,1) i4;
// expected-error@-1 {{type 'i1' (aka '__ptrauth_restricted_intptr(1,1,1) long') is already __ptrauth_restricted_intptr-qualified}}
