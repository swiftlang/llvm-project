// RUN: %clang_cc1 -triple arm64-apple-macosx -ftyped-memory-operations -fsyntax-only -verify %s
#define _TYPED_ALLOC(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

void *malloc(unsigned long);
void *typed_malloc(unsigned long, unsigned long long);
// expected-note@-1 {{rewrite target here}}
void *typed_malloc2(unsigned long, unsigned long long);
void *calloc(unsigned long, unsigned long);
void *typed_calloc(unsigned long, unsigned long, unsigned long long);
// expected-note@-1 2 {{rewrite target here}}
void *typed_calloc2(unsigned long, unsigned long, unsigned long long);

// Some function defs that don't match the required interface
void *invalid_typed_malloc1();
// expected-note@-1 {{rewrite target here}}
void *invalid_typed_malloc2(double);
// expected-note@-1 {{rewrite target here}}
int invalid_typed_malloc3(unsigned);
// expected-note@-1 3 {{rewrite target here}}
int invalid_typed_malloc4(__SIZE_TYPE__, unsigned long long);
void* invalid_typed_malloc5(__SIZE_TYPE__, int);

struct Foo {
  static void *malloc(unsigned long);
  // expected-note@-1 {{rewrite target here}}
  template <typename T> static void *template_malloc(__SIZE_TYPE__, unsigned long long);
  // expected-note@-1 {{candidate function template}}
  template <typename T> static void *test_malloc1(T) _TYPED_ALLOC(typed_malloc, 1);
  // expected-error@-1 {{invalid parameter type for inference at index 1. 'T' is not an integer type}}
  static void *typed_malloc_method(unsigned, unsigned long long);
  void *invalid_typed_malloc_method(unsigned, unsigned long long);
  void *method_malloc(unsigned) _TYPED_ALLOC(typed_malloc_method, 1);
  // expected-error@-1 {{typed memory operation 'method_malloc' cannot be an instance method}}
  void *method_malloc2(unsigned) _TYPED_ALLOC(invalid_typed_malloc_method, 1);
  // expected-error@-1 {{call to non-static member function without an object argument}}
  static void *class_typed_malloc(__SIZE_TYPE__, __SIZE_TYPE__);
  static void *class_malloc(__SIZE_TYPE__) _TYPED_ALLOC(class_typed_malloc, 1);
};

template <typename T> struct Bar {
  static void *class_typed_malloc(__SIZE_TYPE__, T);
  // expected-error@-1 {{typed memory operation 'class_typed_malloc' cannot take a dependent type 'T' as its type descriptor parameter; use a 64-bit integer type instead}}
  static void *class_malloc(__SIZE_TYPE__) _TYPED_ALLOC(class_typed_malloc, 1);
};

void *my_malloc2(int size) _TYPED_ALLOC(typed_malloc, 1);
// expected-error@-1 {{parameter 1 of rewrite target 'typed_malloc' must have type 'int' to match 'my_malloc2', but has type 'unsigned long'}}
void *my_malloc3(double size) _TYPED_ALLOC(typed_malloc, 1);
// expected-error@-1 {{invalid parameter type for inference at index 1. 'double' is not an integer type}}
void *my_malloc4(unsigned size) _TYPED_ALLOC(typed_malloc, -1);
// expected-error@-1 {{'typed_memory_operation' attribute parameter 1 is out of bounds}}
void *my_malloc4(unsigned size) _TYPED_ALLOC(typed_malloc, 0);
// expected-error@-1 {{'typed_memory_operation' attribute parameter 1 is out of bounds}}
void *my_malloc6(unsigned size) _TYPED_ALLOC(typed_malloc, 2);
// expected-error@-1 {{'typed_memory_operation' attribute parameter 1 is out of bounds}}

void *my_malloc_invalid1(unsigned size) _TYPED_ALLOC(invalid_typed_malloc0, 1);
// expected-error@-1 {{use of undeclared identifier 'invalid_typed_malloc0'}}
void *my_malloc_invalid2(unsigned size) _TYPED_ALLOC(invalid_typed_malloc1, 1);
// expected-error@-1 {{rewrite target 'invalid_typed_malloc1' must have 2 parameters to match 'my_malloc_invalid2' plus a type descriptor, but has 0}}
void *my_malloc_invalid3(unsigned size) _TYPED_ALLOC(invalid_typed_malloc2, 1);
// expected-error@-1 {{rewrite target 'invalid_typed_malloc2' must have 2 parameters to match 'my_malloc_invalid3' plus a type descriptor, but has 1}}
void *my_malloc_invalid4(unsigned size) _TYPED_ALLOC(invalid_typed_malloc3, 1);
// expected-error@-1 {{rewrite target 'invalid_typed_malloc3' must return 'void *' to match 'my_malloc_invalid4', but returns 'int'}}
// intentionally using the wrong function
void *my_malloc_invalid5(unsigned size) _TYPED_ALLOC(invalid_typed_malloc3, 1);
// expected-error@-1 {{rewrite target 'invalid_typed_malloc3' must return 'void *' to match 'my_malloc_invalid5', but returns 'int'}}
void *my_malloc_invalid6(unsigned size) _TYPED_ALLOC(invalid_typed_malloc3, 1);
// expected-error@-1 {{rewrite target 'invalid_typed_malloc3' must return 'void *' to match 'my_malloc_invalid6', but returns 'int'}}

void *my_malloc12(unsigned long size) _TYPED_ALLOC(calloc, 1);
void *my_malloc13(unsigned size) _TYPED_ALLOC(typed_calloc, 1);
// expected-error@-1 {{rewrite target 'typed_calloc' must have 2 parameters to match 'my_malloc13' plus a type descriptor, but has 3}}

void *my_malloc14(unsigned size) _TYPED_ALLOC(Foo::malloc, 1);
// expected-error@-1 {{rewrite target 'malloc' must have 2 parameters to match 'my_malloc14' plus a type descriptor, but has 1}}
void *my_malloc15(unsigned size) _TYPED_ALLOC(Foo::template_malloc, 1);
// expected-error@-1 {{no overload of 'template_malloc' has the signature required of a rewrite target for 'my_malloc15'}}
void *my_malloc16(__SIZE_TYPE__ size) _TYPED_ALLOC(Foo::template_malloc<unsigned>, 1);
void *my_malloc17(__SIZE_TYPE__ size) _TYPED_ALLOC(Foo::template_malloc<double>, 1);

void *my_calloc1(unsigned count, unsigned size) _TYPED_ALLOC(typed_calloc, 2);
// expected-error@-1 {{parameter 1 of rewrite target 'typed_calloc' must have type 'unsigned int' to match 'my_calloc1', but has type 'unsigned long'}}

// Overloading vs. rewrite

void alloc_overload_target1(__SIZE_TYPE__, __SIZE_TYPE__, float);
void alloc_overload_target2(__SIZE_TYPE__, __SIZE_TYPE__, int);
void alloc_overload(__SIZE_TYPE__, float) _TYPED_ALLOC(alloc_overload_target1, 1);
void alloc_overload(__SIZE_TYPE__, int) _TYPED_ALLOC(alloc_overload_target2, 1);

// Redeclarations
void *typed_for_redecl(unsigned long long, unsigned long long, unsigned long long);
void *typed_for_redecl2(unsigned long long, unsigned long long, unsigned long long);
void *my_calloc2(unsigned long long count, unsigned long long size) _TYPED_ALLOC(typed_for_redecl, 2);
void *my_calloc2(unsigned long long count, unsigned long long size) _TYPED_ALLOC(typed_for_redecl, 2);
// expected-note@-1 2 {{conflicting attribute is here}}
void *my_calloc2(unsigned long long count, unsigned long long size) _TYPED_ALLOC(typed_for_redecl, 1);
// expected-error@-1 {{attribute 'typed_memory_operation' is already applied with different arguments}}
void *my_calloc2(unsigned long long count, unsigned long long size) _TYPED_ALLOC(typed_for_redecl2, 2);
// expected-error@-1 {{attribute 'typed_memory_operation' is already applied with different arguments}}

void f(){
  my_malloc17(1);
  Bar<float>::class_malloc(10);
  Bar<__SIZE_TYPE__>::class_malloc(10);

}

template <typename... Ts> struct PackHolder {
  static void *typed(__SIZE_TYPE__, unsigned long long, Ts...);
  static void *before_pack(__SIZE_TYPE__, Ts...) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'before_pack' cannot be a variadic template}}
  static void *at_pack(__SIZE_TYPE__, Ts...) _TYPED_ALLOC(typed, 2);
  // expected-error@-1 {{untyped memory operation 'at_pack' cannot be a variadic template}}
  static void *after_pack(Ts..., __SIZE_TYPE__) _TYPED_ALLOC(typed, 2);
  // expected-error@-1 {{untyped memory operation 'after_pack' cannot be a variadic template}}
};

template <typename... Ts> struct PackTarget {
  static void *typed(__SIZE_TYPE__, unsigned long long, Ts...);
  static void *alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{typed memory operation 'typed' cannot be a variadic template}}
};

void *typed_malloc_ull(unsigned long long, unsigned long long);
template <typename T>
void *nonvariadic(unsigned long long) _TYPED_ALLOC(typed_malloc_ull, 1);

template <typename T> struct TemplatedTarget {
  static void *typed(__SIZE_TYPE__, unsigned long long);
  static void *alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
};
template struct TemplatedTarget<int>;

template <typename T> struct DependentMismatch {
  static int *typed(__SIZE_TYPE__, unsigned long long);
  static T *alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{rewrite target 'typed' must return 'double *' to match 'alloc', but returns 'int *'}}
  // expected-note@-3 {{rewrite target here}}
};
template struct DependentMismatch<double>;
// expected-note@-1 {{in instantiation of template class 'DependentMismatch<double>' requested here}}

template <typename T> struct MemberTemplateTarget {
  template <typename U> static T *typed(U, unsigned long long);
  static T *alloc(char) _TYPED_ALLOC(typed<char>, 1);
  // expected-error@-1 {{rewrite target 'typed' cannot be a template specialization that depends on an enclosing template}}
};

struct NonTemplateHolder {
  template <typename U> static void *typed(__SIZE_TYPE__, unsigned long long);
  template <typename U> static U *typed_ret(__SIZE_TYPE__, unsigned long long); // #typed_ret
};
template <typename T> struct DependentTemplateArgument {
  static void *alloc(__SIZE_TYPE__) _TYPED_ALLOC(NonTemplateHolder::typed<T>, 1);
};
template struct DependentTemplateArgument<int>;
template struct DependentTemplateArgument<char>;

template <typename T> struct DependentArgumentMismatch {
  static void *alloc(__SIZE_TYPE__)
      _TYPED_ALLOC(NonTemplateHolder::typed_ret<T>, 1); // #mismatch
  // expected-error@#mismatch {{rewrite target 'typed_ret<int>' must return 'void *' to match 'alloc', but returns 'int *'}}
  // expected-note@#typed_ret {{rewrite target here}}
};
template struct DependentArgumentMismatch<int>; // #mismatch_inst
// expected-note@#mismatch_inst {{in instantiation of template class 'DependentArgumentMismatch<int>' requested here}}

template <typename T> struct ConcreteTarget {
  static void *typed(__SIZE_TYPE__, unsigned long long);
};
void *alloc_from_specialisation(__SIZE_TYPE__)
    _TYPED_ALLOC(ConcreteTarget<int>::typed, 1);

void *typed_tail(unsigned long long, unsigned long long, int);
// expected-note@-1 {{rewrite target here}}
template <typename T>
void *dependent_tail(unsigned long long, T) _TYPED_ALLOC(typed_tail, 1);
// expected-error@-1 {{parameter 3 of rewrite target 'typed_tail' must have type 'double' to match 'dependent_tail<double>', but has type 'int'}}
void *use_tail_valid(unsigned long long n) { return dependent_tail<int>(n, 1); }
void *use_tail_invalid(unsigned long long n) {
  return dependent_tail<double>(n, 1.0);
  // expected-note@-1 {{in instantiation of function template specialization 'dependent_tail<double>' requested here}}
}

