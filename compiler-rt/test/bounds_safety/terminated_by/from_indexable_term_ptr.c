// Test the checks performed on the pointer to the terminator passed to
// `__unsafe_null_terminated_from_indexable(P, PTR_TO_TERM)` and
// `__unsafe_terminated_by_from_indexable(T, P, PTR_TO_TERM)`.
//
// The address space overflow check is tested in
// `from_indexable_term_ptr_overflow.c`.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-below %s %t 1
// RUN: %expect-trap --verify-prefix=null-above %s %t 1 2
// RUN: %expect-trap --verify-prefix=neg1-below %s %t 1 2 3
// RUN: %expect-trap --verify-prefix=neg1-above %s %t 1 2 3 4
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// null-below-merged{convert_null}
// null-above-merged{convert_null}
const char *__null_terminated __attribute__((noinline))
convert_null(const char *__counted_by(len) str, size_t len,
             const char *__single end) {
  // null-below@+2{converting 'const char *__indexable' to 'const char *__null_terminated' where pointer 'end' to the null terminator is below the lower bound of 'str'}
  // null-above@+1{converting 'const char *__indexable' to 'const char *__null_terminated' where pointer 'end' to the null terminator is above the upper bound of 'str'}
  return __unsafe_null_terminated_from_indexable(str, end);
}

// neg1-below-merged{convert_neg1}
// neg1-above-merged{convert_neg1}
int *__terminated_by(-1) __attribute__((noinline))
convert_neg1(int *__counted_by(len) buf, size_t len, int *end) {
  // neg1-below@+2{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'end' to the terminator '-1' is below the lower bound of 'buf'}
  // neg1-above@+1{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'end' to the terminator '-1' is above the upper bound of 'buf'}
  return __unsafe_terminated_by_from_indexable(-1, buf, end);
}

int main(int argc, const char **__counted_by(argc) argv) {
  // The converted pointers start at index 1 of these arrays. Every pointer to
  // the terminator used below points at a terminator so that in soft trap mode
  // execution can continue after the failed check without hitting any other
  // check.
  char str[] = {'\0', 'a', 'b', '\0'};
  int buf[] = {-1, 1, 2, -1};
  const char *__null_terminated null_result = "";
  int *__terminated_by(-1) neg1_result = 0;
  switch (argc) {
  case 1:
    // In bounds: all the checks are performed but none of them fail.
    null_result = convert_null(&str[1], 3, &str[3]);
    neg1_result = convert_neg1(&buf[1], 3, &buf[3]);
    break;
  case 2:
    // The pointer to the terminator is below the converted pointer.
    null_result = convert_null(&str[1], 3, &str[0]);
    break;
  case 3:
    // The pointer to the terminator is at the upper bound of the converted
    // pointer.
    null_result = convert_null(&str[1], 2, &str[3]);
    break;
  case 4:
    neg1_result = convert_neg1(&buf[1], 3, &buf[0]);
    break;
  default:
    neg1_result = convert_neg1(&buf[1], 2, &buf[3]);
    break;
  }
  printf("null_result: '%c'\n", *null_result);
  if (neg1_result)
    printf("neg1_result: %d\n", *neg1_result);
  return 0;
}
