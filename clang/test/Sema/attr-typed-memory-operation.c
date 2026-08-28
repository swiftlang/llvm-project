// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -Rtmo-remarks -fsyntax-only -verify %s
// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify %s

#define _TYPED_ALLOC(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

void *typed_alloc1(__SIZE_TYPE__ size, unsigned long long descriptor);
// expected-note@-1 {{rewrite target}}
// expected-note@-2 {{rewrite target}}
void typed_alloc2(__SIZE_TYPE__ size, unsigned long long descriptor, void **out);
// expected-note@-1 {{rewrite target}}
void typed_alloc3(void** out, __SIZE_TYPE__ size, unsigned long long descriptor);
void *incorrect_descriptor(__SIZE_TYPE__ size, char descriptor);
// expected-note@-1 {{rewrite target here}}

void *alloc1(__SIZE_TYPE__ size) _TYPED_ALLOC(typed_alloc1, 1);
void *alloc2(__SIZE_TYPE__ size) _TYPED_ALLOC(incorrect_descriptor, 1);
// expected-error@-1 {{type descriptor parameter 2 of rewrite target 'incorrect_descriptor' must be a 64-bit integer type, but has type 'char'}}
void *alloc3(__SIZE_TYPE__ size) _TYPED_ALLOC(missing, 1);
// expected-error@-1 {{use of undeclared identifier 'missing'}}
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

int wrong_thing;
void alloc10(__SIZE_TYPE__) _TYPED_ALLOC(wrong_thing, 1);
// expected-error@-1 {{typed memory operation 'wrong_thing' must be a function}}

void alloc11(__SIZE_TYPE__) _TYPED_ALLOC(1, 1);
// expected-error@-1 {{typed memory operation '1' must be a function}}

typedef int __attribute__((vector_size(16))) ivector16;
typedef int __attribute__((vector_size(16))) ivector16_2;
typedef int __attribute__((vector_size(32))) ivector32;

ivector16 *typed_ivalloc(__SIZE_TYPE__, unsigned long long descriptor);
// expected-note@-1 2 {{rewrite target here}}
ivector16 *ivalloc1(__SIZE_TYPE__) _TYPED_ALLOC(typed_ivalloc, 1);
ivector16_2 *ivalloc2(__SIZE_TYPE__) _TYPED_ALLOC(typed_ivalloc, 1);
ivector32 *ivalloc3(__SIZE_TYPE__) _TYPED_ALLOC(typed_ivalloc, 1);
// expected-error@-1 {{rewrite target 'typed_ivalloc' must return 'ivector32 *' to match 'ivalloc3', but returns 'ivector16 *'}}
int *ivalloc4(__SIZE_TYPE__) _TYPED_ALLOC(typed_ivalloc, 1);
// expected-error@-1 {{rewrite target 'typed_ivalloc' must return 'int *' to match 'ivalloc4', but returns 'ivector16 *'}}

// clang doesn't see pointer alignment as part of the type, so these
// types are considered the same. It's possible we'll want to check
// for this in future given we are after all an allocation related
// attribute.
typedef int *__attribute__((aligned(16))) aligned16_ptr;
typedef int *__attribute__((aligned(32))) aligned32_ptr;

aligned16_ptr typed_aligned_alloc(__SIZE_TYPE__, unsigned long long descriptor);
aligned16_ptr aligned_alloc16(__SIZE_TYPE__) _TYPED_ALLOC(typed_aligned_alloc, 1);
aligned32_ptr aligned_alloc32(__SIZE_TYPE__) _TYPED_ALLOC(typed_aligned_alloc, 1);

typedef int _Atomic * atomic_ptr;
atomic_ptr typed_atomic_alloc(__SIZE_TYPE__, unsigned long long descriptor);
// expected-note@-1 {{rewrite target here}}
atomic_ptr atomic_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_atomic_alloc, 1);
int *unatomic_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_atomic_alloc, 1);
// expected-error@-1 {{rewrite target 'typed_atomic_alloc' must return 'int *' to match 'unatomic_alloc', but returns 'atomic_ptr' (aka '_Atomic(int) *')}}

void *typed_bitint_alloc(__SIZE_TYPE__, _BitInt(64));
void *bitint_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_bitint_alloc, 1);
void *typed_unsigned_bitint_alloc(__SIZE_TYPE__, unsigned _BitInt(64));
void *unsigned_bitint_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_unsigned_bitint_alloc, 1);

typedef _BitInt(64) tmo_bi64_t;
void *typed_bitint_typedef_alloc(__SIZE_TYPE__, tmo_bi64_t);
void *bitint_typedef_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_bitint_typedef_alloc, 1);

void *typed_bitint_size_alloc(_BitInt(64), unsigned long long);
void *bitint_size_alloc(_BitInt(64)) _TYPED_ALLOC(typed_bitint_size_alloc, 1);

void *typed_narrow_bitint_alloc(__SIZE_TYPE__, _BitInt(32));
// expected-note@-1 {{rewrite target here}}
void *narrow_bitint_alloc(__SIZE_TYPE__) _TYPED_ALLOC(typed_narrow_bitint_alloc, 1);
// expected-error@-1 {{type descriptor parameter 2 of rewrite target 'typed_narrow_bitint_alloc' must be a 64-bit integer type, but has type '_BitInt(32)'}}

