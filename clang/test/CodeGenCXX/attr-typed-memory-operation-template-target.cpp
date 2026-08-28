// RUN: %clang_cc1 -ftyped-memory-operations -std=c++20 -triple arm64-apple-macosx -nostdsysteminc -O0 -disable-llvm-passes -emit-llvm -o - %s | FileCheck %s \
// RUN:   --implicit-check-not=uncalled_target \
// RUN:   --implicit-check-not=_ZN5Alloc11alloc_typedEmm \
// RUN:   --implicit-check-not=_ZN6Tagged5typedE \
// RUN:   --implicit-check-not=_ZN20MemberTemplateSource5typedE \
// RUN:   --implicit-check-not=_ZN10PackHolder5typedIicE \
// RUN:   --implicit-check-not=_ZN10PackHolder5typedIEE \
// RUN:   --implicit-check-not=_ZN13WidenedHolder5typedIcLi7E \
// RUN:   --implicit-check-not=_ZN13PointerHolder5typedIcLDnE

#define _TYPED(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

struct S {
  int a, b, c;
};

struct Members {
  template <class T> static void *typed(__SIZE_TYPE__ n, unsigned long long d) {
    return (void *)(n + d + sizeof(T));
  }
};
void *member_template(__SIZE_TYPE__) _TYPED(Members::typed<unsigned>, 1);

template <class T> void *free_typed(__SIZE_TYPE__ n, unsigned long long d) {
  return (void *)(n + d + sizeof(T));
}
void *free_template(__SIZE_TYPE__) _TYPED(free_typed<int>, 1);

template <class T> struct Pool {
  static void *typed(__SIZE_TYPE__ n, unsigned long long d);
};
template <class T> void *Pool<T>::typed(__SIZE_TYPE__ n, unsigned long long d) {
  return (void *)(n + d + sizeof(T));
}
void *class_template(__SIZE_TYPE__) _TYPED(Pool<int>::typed, 1);

// A typed memory operation that is never called must not instantiate its
// target.
template <class T> void *uncalled_target(__SIZE_TYPE__ n, unsigned long long d) {
  return (void *)(n + d + sizeof(T));
}
void *never_called(__SIZE_TYPE__) _TYPED(uncalled_target<long>, 1);

S *use_member_template(__SIZE_TYPE__ n) {
  return (S *)member_template(n * sizeof(S));
}
S *use_free_template(__SIZE_TYPE__ n) {
  return (S *)free_template(n * sizeof(S));
}
S *use_class_template(__SIZE_TYPE__ n) {
  return (S *)class_template(n * sizeof(S));
}

// CHECK-LABEL: define {{.*}} @_Z19use_member_templatem
// CHECK: call {{.*}} @_ZN7Members5typedIjEEPvmy(
// CHECK-LABEL: define linkonce_odr {{.*}} @_ZN7Members5typedIjEEPvmy
// CHECK-LABEL: define {{.*}} @_Z17use_free_templatem
// CHECK: call {{.*}} @_Z10free_typedIiEPvmy(
// CHECK-LABEL: define linkonce_odr {{.*}} @_Z10free_typedIiEPvmy
// CHECK-LABEL: define {{.*}} @_Z18use_class_templatem
// CHECK: call {{.*}} @_ZN4PoolIiE5typedEmy(
// CHECK-LABEL: define linkonce_odr {{.*}} @_ZN4PoolIiE5typedEmy

typedef __SIZE_TYPE__ size_t;
typedef size_t malloc_type_id_t;
void *malloc_type_malloc(size_t, malloc_type_id_t);

struct Obj {
  int a, b, c;
};

template <class T> struct Alloc {
  static T *alloc_typed(size_t n, malloc_type_id_t id) {
    return static_cast<T *>(malloc_type_malloc(sizeof(T) * n, id));
  }
  static T *alloc(size_t n) __attribute__((typed_memory_operation(alloc_typed, 1)));
};

Obj *use_obj(size_t n) { return Alloc<Obj>::alloc(n * sizeof(Obj)); }
double *use_double(size_t n) { return Alloc<double>::alloc(n * sizeof(double)); }

template <class T> struct Tagged {
  static void *typed(T, size_t, unsigned long long);
  static void *alloc(T, size_t) __attribute__((typed_memory_operation(typed, 2)));
};
Obj *use_tagged(size_t n) { return (Obj *)Tagged<int>::alloc(1, n * sizeof(Obj)); }

template <class T> struct MemberTemplateSource {
  static T *typed(size_t, unsigned long long);
  template <class U> static T *alloc(size_t) __attribute__((typed_memory_operation(typed, 1)));
};
Obj *use_member_template_source(size_t n) {
  return MemberTemplateSource<Obj>::alloc<char>(n * sizeof(Obj));
}

struct Holder {
  template <class U> static void *typed(size_t, unsigned long long);
};
template <class T> struct Substituted {
  static void *alloc(size_t) __attribute__((typed_memory_operation(Holder::typed<T>, 1)));
};
Obj *use_substituted_int(size_t n) {
  return (Obj *)Substituted<int>::alloc(n * sizeof(Obj));
}
Obj *use_substituted_char(size_t n) {
  return (Obj *)Substituted<char>::alloc(n * sizeof(Obj));
}

struct PackHolder {
  template <class... Us> static void *typed(size_t, unsigned long long);
};
template <class... Ts> struct SubstitutedPack {
  static void *alloc(size_t) __attribute__((typed_memory_operation(PackHolder::typed<Ts...>, 1)));
};
Obj *use_substituted_pack(size_t n) {
  return (Obj *)SubstitutedPack<int, char>::alloc(n * sizeof(Obj));
}
Obj *use_substituted_empty_pack(size_t n) {
  return (Obj *)SubstitutedPack<>::alloc(n * sizeof(Obj));
}

struct WidenedHolder {
  template <class U, long N> static void *typed(size_t, unsigned long long);
};
template <class T, int K> struct SubstitutedWidened {
  static void *alloc(size_t) __attribute__((typed_memory_operation(WidenedHolder::typed<T, K>, 1)));
};
Obj *use_substituted_widened(size_t n) {
  return (Obj *)SubstitutedWidened<char, 7>::alloc(n * sizeof(Obj));
}

struct PointerHolder {
  template <class U, int *P> static void *typed(size_t, unsigned long long);
};
template <class T, int *P> struct SubstitutedPointer {
  static void *alloc(size_t) __attribute__((typed_memory_operation(PointerHolder::typed<T, P>, 1)));
};
Obj *use_substituted_pointer(size_t n) {
  return (Obj *)SubstitutedPointer<char, nullptr>::alloc(n * sizeof(Obj));
}

void reference_targets(size_t n) {
  PackHolder::typed<int, char>(n, 0);
  PackHolder::typed<>(n, 0);
  WidenedHolder::typed<char, 7>(n, 0);
  PointerHolder::typed<char, nullptr>(n, 0);
}

// CHECK-LABEL: define {{.*}} @_Z7use_objm
// CHECK: call {{.*}} @_ZN5AllocI3ObjE11alloc_typedEmm(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
// CHECK-LABEL: define linkonce_odr {{.*}} @_ZN5AllocI3ObjE11alloc_typedEmm
// CHECK-LABEL: define {{.*}} @_Z10use_doublem
// CHECK: call {{.*}} @_ZN5AllocIdE11alloc_typedEmm(i64 noundef %{{.*}}, i64 noundef 72058143796969239)
// CHECK-LABEL: define linkonce_odr {{.*}} @_ZN5AllocIdE11alloc_typedEmm

// CHECK-LABEL: define {{.*}} @_Z10use_taggedm
// CHECK: call {{.*}} @_ZN6TaggedIiE5typedEimy(i32 noundef 1, i64 noundef %{{.*}}, i64 noundef 72058144835799977)

// CHECK-LABEL: define {{.*}} @_Z26use_member_template_sourcem
// CHECK: call {{.*}} @_ZN20MemberTemplateSourceI3ObjE5typedEmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)

// CHECK-LABEL: define {{.*}} @_Z19use_substituted_intm
// CHECK: call {{.*}} @_ZN6Holder5typedIiEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
// CHECK-LABEL: define {{.*}} @_Z20use_substituted_charm
// CHECK: call {{.*}} @_ZN6Holder5typedIcEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)

// CHECK-LABEL: define {{.*}} @_Z20use_substituted_packm
// CHECK: call {{.*}} @_ZN10PackHolder5typedIJicEEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
// CHECK-LABEL: define {{.*}} @_Z26use_substituted_empty_packm
// CHECK: call {{.*}} @_ZN10PackHolder5typedIJEEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
// CHECK-LABEL: define {{.*}} @_Z23use_substituted_widenedm
// CHECK: call {{.*}} @_ZN13WidenedHolder5typedIcLl7EEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
// CHECK-LABEL: define {{.*}} @_Z23use_substituted_pointerm
// CHECK: call {{.*}} @_ZN13PointerHolder5typedIcLPi0EEEPvmy(i64 noundef %{{.*}}, i64 noundef 72058144835799977)
