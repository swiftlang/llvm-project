// RUN: %clang_cc1 -triple arm64-apple-ios -fsyntax-only -verify -fptrauth-intrinsics %s
#if __has_feature(ptrauth_restricted_intptr_qualifier)
int *__ptrauth_restricted_intptr(0) a;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'int *'}}

char __ptrauth_restricted_intptr(0) b;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'char'}}
unsigned char __ptrauth_restricted_intptr(0) c;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'unsigned char'}}
short __ptrauth_restricted_intptr(0) d;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'short'}}
unsigned short __ptrauth_restricted_intptr(0) e;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'unsigned short'}}
int __ptrauth_restricted_intptr(0) f;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'int'}}
unsigned int __ptrauth_restricted_intptr(0) g;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'unsigned int'}}
__int128_t __ptrauth_restricted_intptr(0) h;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is '__int128_t' (aka '__int128')}}
unsigned short __ptrauth_restricted_intptr(0) i;
// expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'unsigned short'}}

unsigned long long __ptrauth_restricted_intptr(0) j;
long long __ptrauth_restricted_intptr(0) k;
__SIZE_TYPE__ __ptrauth_restricted_intptr(0) l;
const unsigned long long __ptrauth_restricted_intptr(0) m;
const long long __ptrauth_restricted_intptr(0) n;
const __SIZE_TYPE__ __ptrauth_restricted_intptr(0) o;

struct S1 {
  __SIZE_TYPE__ __ptrauth_restricted_intptr(0) f0;
};
struct S2 {
  int *__ptrauth_restricted_intptr(0) f0;
  // expected-error@-1{{__ptrauth_restricted_intptr qualifier may only be applied to pointer sized integer types; type here is 'int *'}}
};

void x(unsigned long long __ptrauth_restricted_intptr(0) f0);
// expected-error@-1{{parameter types may not be qualified with __ptrauth_restricted_intptr; type is '__ptrauth_restricted_intptr(0,0,0) unsigned long long'}}

unsigned long long __ptrauth_restricted_intptr(0) y();
// expected-error@-1{{return types may not be qualified with __ptrauth_restricted_intptr; type is '__ptrauth_restricted_intptr(0,0,0) unsigned long long'}}
#endif