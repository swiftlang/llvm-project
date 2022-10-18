// This patch tests that non-affecting files are serialized in such way that
// does not break subsequent source locations (e.g. in serialized pragma
// diagnostic mappings).

// RUN: rm -rf %t
// RUN: split-file %s %t

//--- tu.m
#include "first.h"

//--- first/module.modulemap
module first { header "first.h" }
//--- first/first.h
@import second;
#pragma clang diagnostic ignored "-Wunreachable-code"

//--- second/extra/module.modulemap
module second_extra {}
//--- second/module.modulemap
module second { module sub { header "second_sub.h" } }
//--- second/second_sub.h
@import third;

//--- third/module.modulemap
module third { header "third.h" }
//--- third/third.h
#if __has_feature(nullability)
#endif
#if __has_feature(nullability)
#endif

// RUN: %clang_cc1 -I %t/first -I %t/second -I %t/third -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/cache %t/tu.m -o %t/tu.o
