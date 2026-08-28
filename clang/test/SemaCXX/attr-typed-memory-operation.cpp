// RUN: %clang_cc1 -Rtmo-remarks -ftyped-memory-operations -triple arm64-apple-macosx -std=c++23 -fsyntax-only -verify=expected,remarks %s
// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -std=c++23 -fsyntax-only -verify=expected %s
// RUN: %clang_cc1 -Rtmo-remarks -ftyped-memory-operations -triple arm64-apple-macosx -std=c++23 -fsyntax-only \
// RUN:            -Wtyped-memory-inference-failure -verify=expected,remarks,failure %s

#define _TYPED_ALLOC(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

void *typed_alloc1(__SIZE_TYPE__ size, unsigned long long descriptor);
// expected-note@-1 3 {{rewrite target here}}
void typed_alloc2(__SIZE_TYPE__ size, unsigned long long descriptor, void **out);
// expected-note@-1 {{rewrite target here}}
void typed_alloc3(void** out, __SIZE_TYPE__ size, unsigned long long descriptor);
// expected-note@-1 {{rewrite target here}}
void *incorrect_descriptor(__SIZE_TYPE__ size, char descriptor);
// expected-note@-1 {{rewrite target here}}
void *typed_alloc_to_shadow(__SIZE_TYPE__ size, unsigned long long);
void *typed_alloc_to_shadow(__SIZE_TYPE__ size, float descriptor);
template <typename T> T* templated_typed_alloc1(__SIZE_TYPE__ size, unsigned long long);
// expected-note@-1 {{rewrite target here}}
template <typename T> void* templated_typed_alloc2(__SIZE_TYPE__ size, T);
// expected-note@-1 {{rewrite target here}}
template <typename T> void* templated_typed_alloc3(T size, unsigned long long);
// expected-note@-1 3 {{rewrite target here}}

void *alloc1(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc1, 1);
void *alloc2(__SIZE_TYPE__ size) _TYPED_ALLOC(incorrect_descriptor, 1);
// expected-error@-1 {{type descriptor parameter 2 of rewrite target 'incorrect_descriptor' must be a 64-bit integer type, but has type 'char'}}
void *alloc3(__SIZE_TYPE__ size) _TYPED_ALLOC(missing_func, 1);
// expected-error@-1 {{use of undeclared identifier 'missing_func'}}
void *alloc4(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc2, 1);
// expected-error@-1 {{rewrite target 'typed_alloc2' must return 'void *' to match 'alloc4', but returns 'void'}}
void alloc5(__SIZE_TYPE__ size, void **out) _TYPED_ALLOC(typed_alloc2, 1);
void alloc6(__SIZE_TYPE__ size, void **out) _TYPED_ALLOC(typed_alloc2, 2);
// expected-error@-1 {{invalid parameter type for inference at index 2. 'void **' is not an integer type}}
void alloc7(__SIZE_TYPE__ size, void **out) _TYPED_ALLOC(typed_alloc1, 1);
// expected-error@-1 {{rewrite target 'typed_alloc1' must return 'void' to match 'alloc7', but returns 'void *'}}
void alloc8(void **out, __SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc3, 2);
void alloc9(void **out, __SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc1, 2);
// expected-error@-1 {{rewrite target 'typed_alloc1' must return 'void' to match 'alloc9', but returns 'void *'}}
// alloc10 resolves: only one overload has a 64-bit integer descriptor.
void *alloc10(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc_to_shadow, 1);
int *alloc11(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc1<int>, 1);
float *alloc12(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc1<int>, 1);
// expected-error@-1 {{rewrite target 'templated_typed_alloc1<int>' must return 'float *' to match 'alloc12', but returns 'int *'}}
void *alloc13(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc2<unsigned long long>, 1);
void *alloc14(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc2<int>, 1);
// expected-error@-1 {{type descriptor parameter 2 of rewrite target 'templated_typed_alloc2<int>' must be a 64-bit integer type, but has type 'int'}}
void *alloc15(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc3<unsigned long long>, 1);
// expected-error@-1 {{parameter 1 of rewrite target 'templated_typed_alloc3<unsigned long long>' must have type 'unsigned long' to match 'alloc15', but has type 'unsigned long long'}}
void *alloc16(__SIZE_TYPE__ size) _TYPED_ALLOC(templated_typed_alloc3<unsigned>, 1);
// expected-error@-1 {{parameter 1 of rewrite target 'templated_typed_alloc3<unsigned int>' must have type 'unsigned long' to match 'alloc16', but has type 'unsigned int'}}
void *alloc17(int size) _TYPED_ALLOC(templated_typed_alloc3<unsigned>, 1);
// expected-error@-1 {{parameter 1 of rewrite target 'templated_typed_alloc3<unsigned int>' must have type 'int' to match 'alloc17', but has type 'unsigned int'}}
void *alloc18(int size) _TYPED_ALLOC(typed_alloc1, 1);
// expected-error@-1 {{parameter 1 of rewrite target 'typed_alloc1' must have type 'int' to match 'alloc18', but has type 'unsigned long'}}
void alloc19(char** out, __SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc3, 2);
// expected-error@-1 {{parameter 1 of rewrite target 'typed_alloc3' must have type 'char **' to match 'alloc19', but has type 'void **'}}

void *typed_bitint_alloc(__SIZE_TYPE__, _BitInt(64));
void *alloc20(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_bitint_alloc, 1);
void *typed_unsigned_bitint_alloc(__SIZE_TYPE__, unsigned _BitInt(64));
void *alloc21(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_unsigned_bitint_alloc, 1);

// The rewrite target's accessibility is decided in the context of the
// declaration carrying the attribute, not of any caller of it. The accepted
// cases pin a remark: -verify alone cannot tell an allowed retarget from the
// attribute being silently dropped.

struct AccessObj {
  int a, b, c;
};

struct InsideOk {
private:
  static void *target(int);
  static void *target(__SIZE_TYPE__, unsigned long long);
public:
  static void *source(__SIZE_TYPE__) _TYPED_ALLOC(target, 1);
};
AccessObj *use_inside(__SIZE_TYPE__ n) {
  return (AccessObj *)InsideOk::source(n * sizeof(AccessObj)); // #inside
  // remarks-remark@#inside {{passing TMO information for array of type 'AccessObj' to 'target' (retargeted from 'source')}}
  // remarks-note@#inside {{inferred array of 'AccessObj' from expression 'n * sizeof(AccessObj)'}}
  // remarks-note@#inside {{encoding array of 'AccessObj' as 72058144835799977. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 1042058153 }}}
}

class FriendOk {
  static void *target(int);
  static void *target(__SIZE_TYPE__, unsigned long long);
  friend void *friend_source(__SIZE_TYPE__);
};
void *friend_source(__SIZE_TYPE__) _TYPED_ALLOC(FriendOk::target, 1);
AccessObj *use_friend(__SIZE_TYPE__ n) {
  return (AccessObj *)friend_source(n * sizeof(AccessObj)); // #friend
  // remarks-remark@#friend {{passing TMO information for array of type 'AccessObj' to 'target' (retargeted from 'friend_source')}}
  // remarks-note@#friend {{inferred array of 'AccessObj' from expression 'n * sizeof(AccessObj)'}}
  // remarks-note@#friend {{encoding array of 'AccessObj' as 72058144835799977. { "Summary": { "LayoutSemantics": [ "GenericData" ], "TypeFlags": [ ], "TypeKind": "KindC", "CallsiteFlags": [ "Array" ] }, "TypeHash": 1042058153 }}}
}

class OutsideBad {
  static void *target(int);
  static void *target(__SIZE_TYPE__, unsigned long long);
  // expected-note@-1 {{implicitly declared private here}}
public:
  static void call_from_inside();
};
void *stranger_source(__SIZE_TYPE__) _TYPED_ALLOC(OutsideBad::target, 1);
// expected-error@-1 {{'target' is a private member of 'OutsideBad'}}

// The target being accessible at the callsite does not rescue it, and no
// second diagnostic is produced here.
void OutsideBad::call_from_inside() { stranger_source(1 /* #stranger_size */); } // #stranger_call
// failure-warning@#stranger_call {{could not infer allocation type in call to 'stranger_source'}}
// failure-note@#stranger_size {{unable to infer allocation type from expression '1'}}

struct DeletedOverload {
  static void *target(int);
  static void *target(__SIZE_TYPE__, unsigned long long) = delete;
  // expected-note@-1 {{'target' has been explicitly marked deleted here}}
  static void *source(__SIZE_TYPE__) _TYPED_ALLOC(target, 1);
  // expected-error@-1 {{attempt to use a deleted function}}
};

// The attribute inserts the type descriptor at a fixed argument position, so
// operator functions are rejected: a static operator() or operator[] passes the
// object as argument 0 without it being a parameter, and a new-expression is
// not a call node.

void *typed_malloc(__SIZE_TYPE__, unsigned long long);
void *typed_malloc_ull(unsigned long long, unsigned long long);

struct Members {
  static void *typed(__SIZE_TYPE__, unsigned long long);

  static void *operator()(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator()' cannot be an overloaded operator}}
  static void *operator[](__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator[]' cannot be an overloaded operator}}

  void *operator new(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator new' cannot be an overloaded operator}}
  void *operator new[](__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator new[]' cannot be an overloaded operator}}
  void operator delete(void *) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator delete' cannot be an overloaded operator}}
  void operator delete[](void *) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator delete[]' cannot be an overloaded operator}}

  void *operator+(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator+' cannot be an overloaded operator}}

  void *plain(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'plain' cannot be an instance method}}

  // A static member function that is not an operator is supported.
  static void *alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
};

// A non-member operator has no argument-index skew, but the descriptor kind it
// would encode is only defined for allocation operators and plain calls.
struct Tag {};
void *typed_plus(Tag, __SIZE_TYPE__, unsigned long long);
void *operator+(Tag, __SIZE_TYPE__) _TYPED_ALLOC(typed_plus, 2);
// expected-error@-1 {{untyped memory operation 'operator+' cannot be an overloaded operator}}

// A global allocation function, likewise.
void *operator new(__SIZE_TYPE__, Tag) _TYPED_ALLOC(typed_malloc, 1);
// expected-error@-1 {{untyped memory operation 'operator new' cannot be an overloaded operator}}

// A literal operator is not an operator function: its declarator-id is a
// literal-operator-id, and its argument indices are not skewed.
void *operator""_alloc(unsigned long long) _TYPED_ALLOC(typed_malloc_ull, 1);

// Constructors, destructors and conversion functions cannot be static, so they
// are still caught by the instance-method rejection.
struct SpecialMembers {
  static void *typed(__SIZE_TYPE__, unsigned long long);
  SpecialMembers(__SIZE_TYPE__) _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'SpecialMembers' cannot be an instance method}}
  ~SpecialMembers() _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation '~SpecialMembers' cannot be an instance method}}
  operator void *() _TYPED_ALLOC(typed, 1);
  // expected-error@-1 {{untyped memory operation 'operator void *' cannot be an instance method}}
};

void *typed_malloc_target(__SIZE_TYPE__, unsigned long long);
void *malloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_malloc_target, 1);

__SIZE_TYPE__ runtime_size();
template <class T> void *variable_template = malloc(runtime_size()); // #vt

void *use_variable_template[] = {variable_template<int>,     // #use_int
                                 variable_template<long>};   // #use_long
// failure-note@#use_int {{in instantiation of variable template specialization 'variable_template<int>' requested here}}
// failure-note@#use_long {{in instantiation of variable template specialization 'variable_template<long>' requested here}}
// failure-warning@#vt 2 {{could not infer allocation type in call to 'malloc'}}
// failure-note@#vt 2 {{unable to infer allocation type from expression 'runtime_size()'}}

// Never instantiated: there is no callsite to report.
template <class T> void *unused_variable_template = malloc(runtime_size());

struct Plain {
  template <class T> static void *member;
};
template <class T> void *Plain::member = malloc(runtime_size()); // #plain

void *use_plain_member = Plain::member<int>; // #use_plain
// failure-note@#use_plain {{in instantiation of static data member 'Plain::member<int>' requested here}}
// failure-warning@#plain {{could not infer allocation type in call to 'malloc'}}
// failure-note@#plain {{unable to infer allocation type from expression 'runtime_size()'}}

struct Sizer {
  static __SIZE_TYPE__ size();
};
template <class T> void *dependent_template = malloc(T::size()); // #dependent

void *use_dependent = dependent_template<Sizer>; // #use_dependent
// failure-note@#use_dependent {{in instantiation of variable template specialization 'dependent_template<Sizer>' requested here}}
// failure-warning@#dependent {{could not infer allocation type in call to 'malloc'}}
// failure-note@#dependent {{unable to infer allocation type from expression 'Sizer::size()'}}

template <class T> struct Outer {
  template <class U> static void *member;
};
template <class T> template <class U>
void *Outer<T>::member = malloc(runtime_size()); // #outer

void *use_outer = Outer<int>::member<char>; // #use_outer
// failure-note@#use_outer {{in instantiation of static data member 'Outer<int>::member<char>' requested here}}
// failure-warning@#outer {{could not infer allocation type in call to 'malloc'}}
// failure-note@#outer {{unable to infer allocation type from expression 'runtime_size()'}}

void *ordinary_variable = malloc(runtime_size());
// failure-warning@-1 {{could not infer allocation type in call to 'malloc'}}
// failure-note@-2 {{unable to infer allocation type from expression 'runtime_size()'}}
