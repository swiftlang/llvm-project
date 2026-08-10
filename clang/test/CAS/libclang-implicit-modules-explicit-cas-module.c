// Implicit modules build that also loads an explicit CAS-backed module file,
// driven through libclang. Outer.pcm records Inner's cache key, so Inner is
// read out of the CAS rather than from disk.

// RUN: rm -rf %t
// RUN: split-file %s %t

// RUN: sed -e "s|DIR|%/t|g" %t/cdb.json.template > %t/cdb.json
// RUN: clang-scan-deps -compilation-database %t/cdb.json \
// RUN:   -format experimental-include-tree-full -cas-path %t/cas > %t/deps.json

// RUN: %deps-to-rsp %t/deps.json --module-name Inner > %t/Inner.rsp
// RUN: %deps-to-rsp %t/deps.json --module-name Outer > %t/Outer.rsp
// RUN: %clang @%t/Inner.rsp
// RUN: %clang @%t/Outer.rsp -o %t/Outer.pcm

// RUN: c-index-test -test-load-source all -fmodules -fimplicit-module-maps \
// RUN:   -fmodules-cache-path=%t/mcp -I %t -fmodule-file=Outer=%t/Outer.pcm \
// RUN:   -Xclang -fcas-path -Xclang %t/cas %t/tu.c 2>&1 | FileCheck %s

// CHECK: inner.h:1:12: VarDecl=inner_var
// CHECK: inner.h:2:8: StructDecl=InnerType
// CHECK: outer.h:2:12: VarDecl=outer_var
// CHECK: tu.c:2:5: FunctionDecl=use
// CHECK: tu.c:2:24: TypeRef=struct InnerType
// CHECK: tu.c:2:43: DeclRefExpr=inner_var
// CHECK: tu.c:2:55: DeclRefExpr=outer_var

//--- cdb.json.template
[
  {
    "directory": "DIR",
    "command": "clang -fsyntax-only -fmodules DIR/tu.c -I DIR -fmodules-cache-path=DIR/module-cache",
    "file": "DIR/tu.c"
  }
]

//--- module.modulemap
module Inner { header "inner.h" }
module Outer { header "outer.h" export * }

//--- inner.h
extern int inner_var;
struct InnerType { int x; };

//--- outer.h
#include "inner.h"
extern int outer_var;

//--- tu.c
#include "outer.h"
int use(void) { struct InnerType t; t.x = inner_var + outer_var; return t.x; }
