// RUN: rm -rf %t
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache -x objective-c -I%S/Inputs/merge-in-c/tag-types -verify %s

@import Y;
@import X;

void foo() {
  struct Z z;
  z.m = 2.0;
}

// expected-error@y.h:2 {{type 'struct Z' has incompatible definitions in different translation units}}
// expected-note@y.h:3 {{field 'm' has type 'double' here}}
// expected-note@x.h:3 {{field 'm' has type 'int' here}}
