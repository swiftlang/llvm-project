// RUN: %clang_cc1 -std=c17 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=knr %s
// RUN: %clang_cc1 -std=c23 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=proto %s

#define _TYPED_ALLOC(rewrite_target, type_param_pos) __attribute__((typed_memory_operation(rewrite_target, type_param_pos)))

void *typed_knr(__SIZE_TYPE__, unsigned long long);

void *knr_malloc() _TYPED_ALLOC(typed_knr, 1);
// knr-error@-1 {{untyped memory operation 'knr_malloc' must have a prototype}}
// proto-error@-2 {{'typed_memory_operation' attribute parameter 1 is out of bounds}}

void *variadic_malloc(__SIZE_TYPE__ size, ...) _TYPED_ALLOC(typed_knr, 1);
// knr-error@-1 {{untyped memory operation 'variadic_malloc' cannot be a variadic function}}
// proto-error@-2 {{untyped memory operation 'variadic_malloc' cannot be a variadic function}}

#if __STDC_VERSION__ < 202311L
// A K&R definition has a prototype synthesised from its parameter
// declarations, so it is rejected for not having written one. C23 removed the
// syntax, so these cases only compile in the knr run.

_TYPED_ALLOC(typed_knr, 1) // #knr_source_attr
void *knr_source_definition(n) __SIZE_TYPE__ n; { return 0; } // #knr_source_defn
// knr-error@#knr_source_attr {{untyped memory operation 'knr_source_definition' must have a prototype}}
// knr-warning@#knr_source_defn {{a function definition without a prototype is deprecated}}

void *knr_target(n, d) __SIZE_TYPE__ n; unsigned long long d; { return 0; } // #knr_target_defn
// knr-warning@#knr_target_defn {{a function definition without a prototype is deprecated}}
void *source_of_knr_target(__SIZE_TYPE__) _TYPED_ALLOC(knr_target, 1); // #knr_target_use
// knr-error@#knr_target_use {{typed memory operation 'knr_target' must have a prototype}}
#endif
