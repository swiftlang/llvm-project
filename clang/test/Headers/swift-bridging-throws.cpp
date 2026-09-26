// RUN: %clang_cc1 -fsyntax-only -verify %s
// RUN: %clang_cc1 -fsyntax-only -verify %s -DUSE_SHIM
// RUN: %clang_cc1 -ast-dump %s | FileCheck %s --check-prefix=SUPPORTED
// RUN: %clang_cc1 -ast-dump %s -D__swift__=60000 -D__swift_cxx_throws__=1 | FileCheck %s --check-prefix=SUPPORTED
// RUN: %clang_cc1 -ast-dump %s -D__swift__=60000 -D__swift_cxx_throws__=1 -DUSE_SHIM | FileCheck %s --check-prefix=SUPPORTED
// RUN: %clang_cc1 -ast-dump %s -D__swift__=60000 | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: %clang_cc1 -ast-dump %s -D__swift__=60000 -DUSE_SHIM | FileCheck %s --check-prefix=UNSUPPORTED
// RUN: %clang_cc1 -ast-dump %s -U__has_attribute '-D__has_attribute(x)=0' -Wno-builtin-macro-redefined | FileCheck %s --check-prefix=EMPTY
// expected-no-diagnostics

#ifdef USE_SHIM
#include <swift/bridging>
#else
#include <swift/bridging.h>
#endif

int readValue() SWIFT_THROWS;

// SUPPORTED-LABEL: FunctionDecl {{.*}} readValue 'int ()'
// SUPPORTED-NEXT: SwiftAttrAttr {{.*}} "import_throws"
// UNSUPPORTED-LABEL: FunctionDecl {{.*}} readValue 'int ()'
// UNSUPPORTED-NEXT: AvailabilityAttr {{.*}} swift {{.*}} Unavailable "SWIFT_THROWS requires Swift C++ exception bridging support"
// EMPTY-LABEL: FunctionDecl {{.*}} readValue 'int ()'
// EMPTY-NOT: SwiftAttrAttr
// EMPTY-NOT: AvailabilityAttr

// The annotation must not prevent ordinary C++ callers from using the function,
// including when compiling a header for an older Swift importer.
int callReadValue() { return readValue(); }
