// Test the bounds checks performed on the pointer passed to
// `__unsafe_null_terminated_from_indexable(P)` and
// `__unsafe_terminated_by_from_indexable(T, P)`.
//
// This test can't be run in soft trap mode because execution continues after
// the failed check:
//
// * When the pointer is above its upper bound the search for the terminator
//   that follows fails its bounds check on every element it visits, so
//   multiple soft traps happen, which isn't supported by `expect_trap.py`.
// * When computing the pointer one past the first element overflows, the
//   search for the terminator dereferences the pointer (which is at the end of
//   the address space), which crashes.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-above %s %t 1
// RUN: %expect-trap --verify-prefix=neg1-above %s %t 1 2
// RUN: %expect-trap --verify-prefix=neg1-overflow %s %t 1 2 3
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`.

// null-above-merged{find_null}
const char *__null_terminated __attribute__((noinline))
find_null(const char *__counted_by(len) str, size_t len) {
  // null-above@+1{converting 'const char *__indexable' to 'const char *__null_terminated' where pointer 'str' is above its upper bound}
  return __unsafe_null_terminated_from_indexable(str);
}

// The element type is larger than 1 byte so the bounds check also checks that
// computing the pointer one past the first element doesn't overflow.

// neg1-above-merged{find_neg1}
// neg1-overflow-merged{find_neg1}
int *__terminated_by(-1) __attribute__((noinline))
find_neg1(int *__counted_by(len) buf, size_t len) {
  // neg1-above@+2{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'buf' is above its upper bound}
  // neg1-overflow@+1{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'buf' overflows the address space}
  return __unsafe_terminated_by_from_indexable(-1, buf);
}

int main(int argc, const char **__counted_by(argc) argv) {
  char str[] = {'a', '\0'};
  int buf[] = {1, -1};
  switch (argc) {
  case 1: {
    // In bounds: the checks are performed but don't fail.
    const char *__null_terminated null_result = find_null(str, 2);
    int *__terminated_by(-1) neg1_result = find_neg1(buf, 2);
    printf("null_result: '%c' neg1_result: %d\n", *null_result, *neg1_result);
    break;
  }
  case 2: {
    // The pointer is at its upper bound.
    const char *__null_terminated null_result = find_null(str, 0);
    printf("null_result: '%c'\n", *null_result);
    break;
  }
  case 3: {
    int *__terminated_by(-1) neg1_result = find_neg1(buf, 0);
    printf("neg1_result: %d\n", *neg1_result);
    break;
  }
  default: {
    // Computing the pointer one past the first element overflows.
    int *__terminated_by(-1) neg1_result = find_neg1(
        __unsafe_forge_bidi_indexable(int *, UINTPTR_MAX - sizeof(int) + 1, 0),
        0);
    printf("neg1_result: %d\n", *neg1_result);
    break;
  }
  }
  return 0;
}
