// RUN: %clang_cc1 -Rtmo-remarks -verify -fsyntax-only -ftyped-memory-operations -triple arm64-apple-macosx -nostdsysteminc -O0 %s
// RUN: %clang_cc1    -ftyped-memory-operations -triple arm64-apple-macosx -nostdsysteminc -O0 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s

#define _TYPED(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

extern "C" void *typed_malloc(__SIZE_TYPE__ size, unsigned long long);
extern "C" void *test_malloc(__SIZE_TYPE__ size) _TYPED(typed_malloc, 1);

struct S {
  void *p;
  int i;
};

template <class T> struct Holder {
  T *P;
  template <bool Dummy = true> explicit Holder(T *P) : P(P) {}
};

struct WithMemberInit {
  Holder<S> H;
  WithMemberInit(__SIZE_TYPE__ N) : H((S *)test_malloc(N)) {} // #member_init
  // expected-remark@#member_init {{passing TMO information for type 'S' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#member_init {{inferred 'S' from cast of result from call to '(S *)test_malloc(N)'}}
  // expected-note@#member_init {{encoding 'S' as 74309671181593780. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2452073652 }}}
};

WithMemberInit make_with_member_init(__SIZE_TYPE__ N) {
  return WithMemberInit(N);
}

Holder<S> in_function(__SIZE_TYPE__ N) {
  return Holder<S>((S *)test_malloc(N)); // #in_function
  // expected-remark@#in_function {{passing TMO information for type 'S' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#in_function {{inferred 'S' from cast of result from call to '(S *)test_malloc(N)'}}
  // expected-note@#in_function {{encoding 'S' as 74309671181593780. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2452073652 }}}
}

struct WithDefaultMemberInit {
  S *P = (S *)test_malloc(sizeof(S)); // #default_member_init
  // expected-remark@#default_member_init {{passing TMO information for type 'S' to 'typed_malloc' (retargeted from 'test_malloc')}}
  // expected-note@#default_member_init {{inferred 'S' from expression 'sizeof(S)'}}
  // expected-note@#default_member_init {{encoding 'S' as 74309671181593780. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2452073652 }}}
};
WithDefaultMemberInit make_with_default_member_init() { return {}; }

void with_default_arg(S *P = (S *)test_malloc(sizeof(S))); // #default_arg
// expected-remark@#default_arg {{passing TMO information for type 'S' to 'typed_malloc' (retargeted from 'test_malloc')}}
// expected-note@#default_arg {{inferred 'S' from expression 'sizeof(S)'}}
// expected-note@#default_arg {{encoding 'S' as 74309671181593780. { "Summary": { "LayoutSemantics": [ "AnonymousPointer", "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "FixedSize" ] }, "TypeHash": 2452073652 }}}
void call_with_default_arg() { with_default_arg(); }

// The checks below are in the order the definitions are emitted in.
// CHECK-LABEL: define {{.*}} @_Z11in_functionm
// CHECK: call ptr @typed_malloc(i64 noundef %{{.*}}, i64 noundef [[S_DESC:74309671181593780]])
// CHECK-LABEL: define {{.*}} @_Z29make_with_default_member_initv
// CHECK: call ptr @typed_malloc(i64 noundef 16, i64 noundef [[S_DESC]])
// CHECK-LABEL: define {{.*}} @_Z21call_with_default_argv
// CHECK: call ptr @typed_malloc(i64 noundef 16, i64 noundef [[S_DESC]])
// CHECK-LABEL: define {{.*}} @_ZN14WithMemberInitC2Em
// CHECK: call ptr @typed_malloc(i64 noundef %{{.*}}, i64 noundef [[S_DESC]])
