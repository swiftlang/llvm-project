// RUN: %clang_cc1 -fsyntax-only -Wno-strict-prototypes -fsuppress-conflicting-types -verify %s
// expected-no-diagnostics

void blapp(int);
void blapp() {}

void yarp(int, ...);
void yarp();

void blarg(int, ...);
void blarg() {}

void blerp(short);
void blerp(x) int x;
{}

void foo(int);
void foo();
void foo() {}
