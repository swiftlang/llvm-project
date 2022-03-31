#include "print-unit.h"
#include "syshead.h"

void foo(int i);

// RUN: rm -rf %t
// RUN: %clang_cc1 -I %S/Inputs -isystem %S/Inputs/sys -fdebug-prefix-map=%S=SRC_ROOT -fdebug-prefix-map=$PWD=BUILD_ROOT -index-store-path %t/idx %s -triple x86_64-apple-macosx10.8
// RUN: c-index-test core -print-unit %t/idx | FileCheck %s

// CHECK: print-unit-remapped.c.o-20EK9G967JO97
// CHECK: provider: clang-
// CHECK: is-system: 0
// CHECK: has-main: 1
// CHECK: main-path: SRC_ROOT{{/|\\}}print-unit-remapped.c
// CHECK: work-dir: BUILD_ROOT
// CHECK: out-file: SRC_ROOT{{/|\\}}print-unit-remapped.c.o
// CHECK: target: x86_64-apple-macosx10.8
// CHECK: is-debug: 1
// CHECK: DEPEND START
// CHECK: Record | user | SRC_ROOT{{/|\\}}print-unit-remapped.c | print-unit-remapped.c-
// CHECK: Record | user | SRC_ROOT{{/|\\}}Inputs{{/|\\}}head.h | head.h-
// CHECK: Record | user | SRC_ROOT{{/|\\}}Inputs{{/|\\}}using-overlay.h | using-overlay.h-
// CHECK: Record | system | SRC_ROOT{{/|\\}}Inputs{{/|\\}}sys{{/|\\}}syshead.h | syshead.h-
// CHECK: Record | system | SRC_ROOT{{/|\\}}Inputs{{/|\\}}sys{{/|\\}}another.h | another.h-
// CHECK: File | user | SRC_ROOT{{/|\\}}Inputs{{/|\\}}print-unit.h{{$}}
// CHECK: DEPEND END (6)
// CHECK: INCLUDE START
// CHECK: SRC_ROOT{{/|\\}}print-unit-remapped.c:1 | SRC_ROOT{{/|\\}}Inputs{{/|\\}}print-unit.h
// CHECK: SRC_ROOT{{/|\\}}print-unit-remapped.c:2 | SRC_ROOT{{/|\\}}Inputs{{/|\\}}sys{{/|\\}}syshead.h
// CHECK: SRC_ROOT{{/|\\}}Inputs{{/|\\}}print-unit.h:1 | SRC_ROOT{{/|\\}}Inputs{{/|\\}}head.h
// CHECK: SRC_ROOT{{/|\\}}Inputs{{/|\\}}print-unit.h:2 | SRC_ROOT{{/|\\}}Inputs{{/|\\}}using-overlay.h
// CHECK: INCLUDE END (4)
