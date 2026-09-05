// RUN: rm -rf %t
// RUN: mkdir -p %t
// RUN: split-file %s %t
//
// RUN: %clang_cc1 -std=c++20 -ftyped-memory-operations -triple arm64-apple-macosx \
// RUN:   -Rtmo-remarks -verify=priv -emit-module-interface -o %t/priv.pcm %t/priv.cppm
//
// No --implicit-check-not=tm_priv here: a call written in the fragment does
// bind to the fragment's own declaration, so tm_priv appears in this run.
// RUN: %clang_cc1 -std=c++20 -ftyped-memory-operations -triple arm64-apple-macosx \
// RUN:   -Rtmo-remarks -emit-llvm -o - %t/priv.cppm \
// RUN:   | FileCheck %s --check-prefix=INMODULE
//
// RUN: %clang_cc1 -std=c++20 -ftyped-memory-operations -triple arm64-apple-macosx \
// RUN:   -Rtmo-remarks -fmodule-file=priv=%t/priv.pcm -emit-llvm -o - %t/user.cpp \
// RUN:   -verify | FileCheck %s --implicit-check-not=tm_priv

//--- priv.cppm
export module priv;

typedef __SIZE_TYPE__ size_t;

void *tm_priv(size_t, unsigned long long);

export void *alloc(size_t);

export struct Shared {
  void *p;
  int i;
};

export template <class T> T *alloc_t(size_t n) {
  return (T *)alloc(n * sizeof(T));
}

module :private;

void *alloc(size_t) __attribute__((typed_memory_operation(tm_priv, 1)));

Shared *force_in_module(size_t n) { return alloc_t<Shared>(n); }

Shared *use_in_fragment(size_t n) {
  return (Shared *)alloc(n * sizeof(Shared)); // #in_fragment
  // priv-remark@#in_fragment {{passing TMO information for array of type 'Shared' to 'tm_priv' (retargeted from 'alloc')}}
  // priv-note@#in_fragment {{inferred array of 'Shared' from expression 'n * sizeof(Shared)'}}
  // priv-note@#in_fragment {{encoding array of 'Shared' as 74309946059500724. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 2452073652 }}}
}

// alloc_t<Shared> is emitted in both translation units, so both must agree.
// INMODULE-LABEL: define linkonce_odr {{.*}} @_ZW4priv7alloc_tIS_6SharedEPT_m
// INMODULE: call {{.*}} @_ZW4priv5allocm

//--- user.cpp
import priv;

typedef __SIZE_TYPE__ size_t;

struct Obj {
  void *p;
  int i;
};

Obj *use(size_t n) { return (Obj *)alloc(n * sizeof(Obj)); }

Shared *use_template(size_t n) { return alloc_t<Shared>(n); }

// expected-no-diagnostics

// CHECK-LABEL: define {{.*}} @_Z3usem
// CHECK: call {{.*}} @_ZW4priv5allocm(i64 noundef %{{.*}})
// CHECK-LABEL: define linkonce_odr {{.*}} @_ZW4priv7alloc_tIS_6SharedEPT_m
// CHECK: call {{.*}} @_ZW4priv5allocm
