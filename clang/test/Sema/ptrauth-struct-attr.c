// RUN: %clang_cc1 -triple arm64-apple-ios -fsyntax-only -verify %s

#include <ptrauth.h>

#define VALID_KEY 2
#define INVALID_KEY 200
#define VALID_DISC 100
#define INVALID_DISC 100000
#define VALID_KEY1 1
#define VALID_DISC1 101

int nonconstval0;

int *__attribute__((ptrauth_struct(VALID_KEY, VALID_DISC))) intPtr; // expected-warning {{only applies to structs, unions, and classes}}

struct __attribute__((ptrauth_struct(nonconstval0, VALID_DISC))) NonConstIntKeyS {};  // expected-error {{not an integer constant expression}}
struct __attribute__((ptrauth_struct(VALID_KEY, nonconstval0))) NonConstIntDiscS0 {}; // expected-error {{ptrauth_struct must be an integer constant expression}}
struct __attribute__((ptrauth_struct(VALID_KEY, "val"))) NonConstIntDiscS1 {};        // expected-error {{ptrauth_struct must be an integer constant expression}}
struct __attribute__((ptrauth_struct(VALID_KEY))) NoDiscS {};                         // expected-error {{requires exactly 2 arguments}}

struct __attribute__((ptrauth_struct(VALID_KEY, VALID_DISC))) ValidS {};          // expected-note {{previous declaration of}}
struct __attribute__((ptrauth_struct(INVALID_KEY, VALID_DISC))) InvalidKeyS {};   // expected-error {{200 does not identify a valid pointer authentication key}}
struct __attribute__((ptrauth_struct(VALID_KEY, INVALID_DISC))) InvalidDiscS0 {}; // expected-error {{for ptrauth_struct must be between 0 and 65535; value is 100000}}

struct __attribute__((ptrauth_struct(VALID_KEY1, VALID_DISC))) ValidS;  // expected-error {{is signed differently from the previous declaration}} expected-note {{previous declaration of}}
struct __attribute__((ptrauth_struct(VALID_KEY1, VALID_DISC1))) ValidS; // expected-error {{is signed differently from the previous declaration}}
struct __attribute__((ptrauth_struct(VALID_KEY1, VALID_DISC1))) ValidS; // expected-note {{previous declaration of}}
struct ValidS; // expected-error {{is signed differently from the previous declaration}}

struct __attribute__((ptrauth_struct(ptrauth_key_none, VALID_DISC))) NoneS {};
struct __attribute__((ptrauth_struct(ptrauth_key_none, VALID_DISC1))) NoneS;
