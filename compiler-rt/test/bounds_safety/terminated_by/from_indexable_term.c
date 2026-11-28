// Test the checks that the terminator is found performed by
// `__unsafe_null_terminated_from_indexable(P [, PTR_TO_TERM])` and
// `__unsafe_terminated_by_from_indexable(T, P [, PTR_TO_TERM])`.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-not-found %s %t 1
// RUN: %expect-trap --verify-prefix=neg1-not-found %s %t 1 2
// RUN: %expect-trap --verify-prefix=null-not-term %s %t 1 2 3
// RUN: %expect-trap --verify-prefix=neg1-not-term %s %t 1 2 3 4
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// null-not-found-merged{find_null}
const char *__null_terminated __attribute__((noinline))
find_null(const char *__counted_by(len) str, size_t len) {
  // null-not-found@+1{converting 'const char *__indexable' to 'const char *__null_terminated' where there is no null terminator within the bounds of 'str'}
  return __unsafe_null_terminated_from_indexable(str);
}

// neg1-not-found-merged{find_neg1}
int *__terminated_by(-1) __attribute__((noinline))
find_neg1(int *__counted_by(len) buf, size_t len) {
  // neg1-not-found@+1{converting 'int *__indexable' to 'int *__terminated_by(-1)' where there is no terminator '-1' within the bounds of 'buf'}
  return __unsafe_terminated_by_from_indexable(-1, buf);
}

// null-not-term-merged{convert_null}
const char *__null_terminated __attribute__((noinline))
convert_null(const char *__counted_by(len) str, size_t len,
             const char *__single end) {
  // null-not-term@+1{converting 'const char *__indexable' to 'const char *__null_terminated' where pointer 'end' does not point to the null terminator}
  return __unsafe_null_terminated_from_indexable(str, end);
}

// neg1-not-term-merged{convert_neg1}
int *__terminated_by(-1) __attribute__((noinline))
convert_neg1(int *__counted_by(len) buf, size_t len, int *end) {
  // neg1-not-term@+1{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'end' does not point to the terminator '-1'}
  return __unsafe_terminated_by_from_indexable(-1, buf, end);
}

int main(int argc, const char **__counted_by(argc) argv) {
  // The last element of these arrays is the terminator. When the bounds passed
  // to the `find_*` functions exclude it, the search for the terminator fails
  // its bounds check on reaching it. In soft trap mode execution continues
  // after the failed check and the search ends there without hitting any other
  // check.
  char str[] = {'a', 'b', '\0'};
  int buf[] = {1, 2, -1};
  const char *__null_terminated null_result = "";
  int *__terminated_by(-1) neg1_result = 0;
  switch (argc) {
  case 1:
    // All the checks are performed but none of them fail.
    null_result = find_null(str, 3);
    neg1_result = find_neg1(buf, 3);
    null_result = convert_null(str, 3, &str[2]);
    neg1_result = convert_neg1(buf, 3, &buf[2]);
    break;
  case 2:
    // The terminator is just past the bounds.
    null_result = find_null(str, 2);
    break;
  case 3:
    neg1_result = find_neg1(buf, 2);
    break;
  case 4:
    // The pointer to the terminator points to an element that isn't the
    // terminator.
    null_result = convert_null(str, 3, &str[1]);
    break;
  default:
    neg1_result = convert_neg1(buf, 3, &buf[1]);
    break;
  }
  printf("null_result: '%c'\n", *null_result);
  if (neg1_result)
    printf("neg1_result: %d\n", *neg1_result);
  return 0;
}
