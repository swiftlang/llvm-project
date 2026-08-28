// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=quiet %s
// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=conflict -Wtyped-memory-inference-conflict %s
// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=failure -Wtyped-memory-inference-failure %s
// RUN: %clang_cc1 -ftyped-memory-operations -triple arm64-apple-macosx -fsyntax-only -verify=conflict,failure -Wtyped-memory-operations %s

// quiet-no-diagnostics

void *typed_malloc(__SIZE_TYPE__, unsigned long long);
void *my_malloc(__SIZE_TYPE__ size)
    __attribute__((typed_memory_operation(typed_malloc, 1)));

struct S { int a; long b; };

void inference_conflict(void) {
  double *d = (double *)my_malloc(sizeof(struct S));
  // conflict-warning@-1 {{size argument of 'my_malloc' implies allocation type 'struct S', which conflicts with the cast of the result to 'double'; encoding 'struct S'}}
  (void)d;
}

void inference_failure(__SIZE_TYPE__ n) {
  void *p = my_malloc(n);
  // failure-warning@-1 {{could not infer allocation type in call to 'my_malloc'}}
  (void)p;
}
