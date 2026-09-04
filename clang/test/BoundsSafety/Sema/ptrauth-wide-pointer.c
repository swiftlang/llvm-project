
// RUN: %clang_cc1 -triple arm64e -fptrauth-intrinsics -fbounds-safety -fsyntax-only -verify %s
// RUN: %clang_cc1 -triple arm64e -fptrauth-intrinsics -fbounds-safety -x objective-c -fexperimental-bounds-safety-objc -fsyntax-only -verify %s

#include <ptrcheck.h>
#include <stddef.h>

int a[10];
struct S { int *__bidi_indexable f; };
int *__bidi_indexable get_bidi(void);
int *__indexable get_indexable(void);

//===----------------------------------------------------------------------===//
// 1. Rejected: bounds held in memory.
//===----------------------------------------------------------------------===//

// A local pointer variable is implicitly __bidi_indexable.
unsigned long local_var(void) {
  int *p = a;
  return __builtin_ptrauth_blend_discriminator(p, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long local_var_explicit_bidi(void) {
  int *__bidi_indexable p = a;
  return __builtin_ptrauth_blend_discriminator(p, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long param_bidi(int *__bidi_indexable p) {
  return __builtin_ptrauth_blend_discriminator(p, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long param_indexable(int *__indexable p) {
  return __builtin_ptrauth_blend_discriminator(p, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long struct_member(struct S *__single s) {
  return __builtin_ptrauth_blend_discriminator(s->f, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long deref(int *__bidi_indexable *__single pp) {
  return __builtin_ptrauth_blend_discriminator(*pp, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

// Casting a wide pointer to another *wide* pointer type does not discard
// anything, so it cannot be used to get past the rule.
unsigned long launder_via_bidi_cast(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator((int *__bidi_indexable)w, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long launder_via_indexable_cast(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator((int *__indexable)w, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

// The rule covers every pointer-accepting slot, not just the value operand.
unsigned long in_generic_slot(int *__bidi_indexable p) {
  return __builtin_ptrauth_sign_generic_data(p, 0);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

int *__single in_disc_slot(int *__single p, int *__bidi_indexable d) {
  return __builtin_ptrauth_auth(p, 2, d);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

int *__single in_value_slot(int *__bidi_indexable p) {
  return __builtin_ptrauth_strip(p, 2);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

//===----------------------------------------------------------------------===//
// The remedies the diagnostic names: an explicit cast to a raw-layout type.
//===----------------------------------------------------------------------===//

unsigned long cast_to_unsafe_ok(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator((int *__unsafe_indexable)w, 42); // no diagnostic
}

unsigned long cast_to_single_ok(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator((int *__single)w, 42); // no diagnostic
}

//===----------------------------------------------------------------------===//
// 2. Accepted: a wide pointer rvalue, with no stored bounds to lose.
//===----------------------------------------------------------------------===//

unsigned long addrof_global_ok(void) {
  return __builtin_ptrauth_blend_discriminator(&a[0], 42); // no diagnostic
}

// `&expr` is accepted regardless of what the address is rooted in, including a
// wide pointer. The bounds fields of the result are synthesised for the object
// being addressed and then thrown away by the builtin either way.
unsigned long addrof_elem_of_wide(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator(&w[0], 42); // no diagnostic
}

unsigned long addrof_deref_of_wide(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator(&*w, 42); // no diagnostic
}

unsigned long addrof_elem_of_indexable(int *__indexable w) {
  return __builtin_ptrauth_blend_discriminator(&w[0], 42); // no diagnostic
}

unsigned long addrof_elem_of_wide_member(struct S *__single s) {
  return __builtin_ptrauth_blend_discriminator(&s->f[0], 42); // no diagnostic
}

unsigned long addrof_elem_of_deref(int *__bidi_indexable *__single pp) {
  return __builtin_ptrauth_blend_discriminator(&(*pp)[0], 42); // no diagnostic
}

// Raw-layout bases likewise, which is the liblibc shape.
unsigned long addrof_elem_of_sized_by(int *__sized_by(n) p, unsigned long n) {
  return __builtin_ptrauth_blend_discriminator(&p[0], 42); // no diagnostic
}

unsigned long addrof_elem_of_counted_by(int *__counted_by(n) p, int n) {
  return __builtin_ptrauth_blend_discriminator(&p[0], 42); // no diagnostic
}

unsigned long addrof_elem_of_single(int *__single p) {
  return __builtin_ptrauth_blend_discriminator(&p[0], 42); // no diagnostic
}

// Addressing a wide pointer object rather than an element of one.
unsigned long addrof_wide_member(struct S *__single s) {
  return __builtin_ptrauth_blend_discriminator(&s->f, 42); // no diagnostic
}

unsigned long addrof_wide_var(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator(&w, 42); // no diagnostic
}

// A wide pointer read only to compute the index is equally irrelevant.
unsigned long addrof_wide_only_in_index(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator(&a[w[0]], 42); // no diagnostic
}

unsigned long addrof_wide_member_in_index(struct S *__single s) {
  return __builtin_ptrauth_blend_discriminator(&a[s->f[0]], 42); // no diagnostic
}


// The liblibc shape that motivated the fix.
void sign_destructors(unsigned long *__sized_by(sz) entries, size_t sz) {
  for (size_t i = 0; i < sz / sizeof(*entries); ++i)
    entries[i] = __builtin_ptrauth_blend_discriminator(&entries[i], 42); // no diagnostic
}

unsigned long sign_generic_rvalues_ok(void) {
  return __builtin_ptrauth_sign_generic_data(&a[0], &a[1]); // no diagnostic
}

// Also rejected: rvalues whose bounds derive from something the program already
// holds. Being an rvalue is not enough -- only `&expr` synthesises its bounds
// here, so these still have to say what they are discarding.
unsigned long ptr_arith(int *__bidi_indexable w) {
  return __builtin_ptrauth_blend_discriminator(w + 1, 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long call_result(void) {
  return __builtin_ptrauth_blend_discriminator(get_bidi(), 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

unsigned long indexable_call_result(void) {
  return __builtin_ptrauth_blend_discriminator(get_indexable(), 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

// A forged pointer carries bounds the programmer stated explicitly, which is all
// the more reason not to drop them silently.
unsigned long forged(void *__single q, unsigned long n) {
  return __builtin_ptrauth_blend_discriminator(
      __unsafe_forge_bidi_indexable(int *, q, n), 42);
  // expected-error@-1{{pointer authentication operates on the address only and would discard the bounds of 'int *__bidi_indexable'; cast to '__single' or '__unsafe_indexable' to discard them explicitly}}
}

//===----------------------------------------------------------------------===//
// Value slot: the result of an accepted wide-pointer operand is raw-layout, so
// its bounds cannot be re-acquired implicitly. This matches every other
// pointer-returning builtin -- __builtin_alloca, __builtin_strchr and friends are
// all __unsafe_indexable -- so a bounded result needs __unsafe_forge_*.
//===----------------------------------------------------------------------===//

int *__unsafe_indexable strip_to_unsafe_ok(void) {
  return __builtin_ptrauth_strip(&a[0], 2); // no diagnostic
}

unsigned long strip_to_integer_ok(void) {
  return (unsigned long)__builtin_ptrauth_strip(&a[0], 2); // no diagnostic
}

void strip_to_forged_bidi_ok(void) {
  int *__bidi_indexable q =
      __unsafe_forge_bidi_indexable(int *, __builtin_ptrauth_strip(&a[0], 2),
                                    sizeof(a)); // no diagnostic
  (void)q;
}

void strip_to_bidi(void) {
  int *__bidi_indexable q = __builtin_ptrauth_strip(&a[0], 2);
  // expected-error@-1{{initializing 'int *__bidi_indexable' with an expression of incompatible type 'int *__unsafe_indexable' casts away '__unsafe_indexable' qualifier; use '__unsafe_forge_single' or '__unsafe_forge_bidi_indexable' to perform this conversion}}
  (void)q;
}

// A local pointer is implicitly __bidi_indexable, so this is the same error.
void strip_to_bidi_impl(void) {
  int *q = __builtin_ptrauth_strip(&a[0], 2);
  // expected-error@-1{{initializing 'int *__bidi_indexable' with an expression of incompatible type 'int *__unsafe_indexable' casts away '__unsafe_indexable' qualifier; use '__unsafe_forge_single' or '__unsafe_forge_bidi_indexable' to perform this conversion}}
  (void)q;
}

void strip_to_single(void) {
  int *__single q = __builtin_ptrauth_strip(&a[0], 2);
  // expected-error@-1{{initializing 'int *__single' with an expression of incompatible type 'int *' casts away '__unsafe_indexable' qualifier; use '__unsafe_forge_single' or '__unsafe_forge_bidi_indexable' to perform this conversion}}
  (void)q;
}

//===----------------------------------------------------------------------===//
// Raw-layout operands are untouched by either rule.
//===----------------------------------------------------------------------===//

unsigned long single_ok(int *__single p) {
  return __builtin_ptrauth_blend_discriminator(p, 42); // no diagnostic
}

unsigned long unsafe_ok(int *__unsafe_indexable p) {
  return __builtin_ptrauth_blend_discriminator(p, 42); // no diagnostic
}

// __counted_by keeps its bounds outside the pointer.
unsigned long counted_by_ok(int *__counted_by(n) p, int n) {
  return __builtin_ptrauth_blend_discriminator(p, 42); // no diagnostic
}

int *__single strip_single_ok(int *__single p) {
  return __builtin_ptrauth_strip(p, 2); // no diagnostic
}

//===----------------------------------------------------------------------===//
// __builtin_ptrauth_sign_constant reaches the same checks (it is handled by
// PointerAuthSignOrAuth with RequireConstant=true). Before the fix a wide-pointer
// operand crashed in the constant-emission path with "Invalid constantexpr
// bitcast!" -- a different symptom of the same missing cast, not a separate bug.
//===----------------------------------------------------------------------===//

void *__unsafe_indexable const sign_const_ok =
    __builtin_ptrauth_sign_constant(&a[0], 2, 0); // no diagnostic

void *const sign_const_to_single = __builtin_ptrauth_sign_constant(&a[0], 2, 0);
// expected-error@-1{{initializing 'void *__singleconst' with an expression of incompatible type 'int *' casts away '__unsafe_indexable' qualifier; use '__unsafe_forge_single' or '__unsafe_forge_bidi_indexable' to perform this conversion}}
