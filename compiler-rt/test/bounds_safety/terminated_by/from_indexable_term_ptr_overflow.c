// Test the address space overflow check performed on the pointer to the
// terminator passed to `__unsafe_null_terminated_from_indexable(P,
// PTR_TO_TERM)` and `__unsafe_terminated_by_from_indexable(T, P, PTR_TO_TERM)`.
//
// In soft trap mode execution continues after the failed check and the
// pointer to the terminator (which is at the end of the address space) is
// dereferenced, which crashes. So this test can't be run in that mode.
// UNSUPPORTED: soft-traps
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-overflow %s %t 1
// RUN: %expect-trap --verify-prefix=neg1-overflow %s %t 1 2
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

// `noinline` here is used to prevent the check from being optimized using the
// values passed by `main`.

// null-overflow-merged{convert_null}
const char *__null_terminated __attribute__((noinline))
convert_null(const char *__counted_by(len) str, size_t len,
             const char *__single end) {
  // null-overflow@+1{converting 'const char *__indexable' to 'const char *__null_terminated' where pointer 'end' to the null terminator overflows the address space}
  return __unsafe_null_terminated_from_indexable(str, end);
}

// neg1-overflow-merged{convert_neg1}
int *__terminated_by(-1) __attribute__((noinline))
convert_neg1(int *__counted_by(len) buf, size_t len, int *end) {
  // neg1-overflow@+1{converting 'int *__indexable' to 'int *__terminated_by(-1)' where pointer 'end' to the terminator '-1' overflows the address space}
  return __unsafe_terminated_by_from_indexable(-1, buf, end);
}

int main(int argc, const char **__counted_by(argc) argv) {
  char str[] = {'a', 'b', '\0'};
  int buf[] = {1, 2, -1};
  switch (argc) {
  case 1: {
    // In bounds: the checks are performed but don't fail.
    const char *__null_terminated null_result = convert_null(str, 3, &str[2]);
    int *__terminated_by(-1) neg1_result = convert_neg1(buf, 3, &buf[2]);
    printf("null_result: '%c' neg1_result: %d\n", *null_result, *neg1_result);
    break;
  }
  case 2: {
    // Computing the pointer one past the terminator overflows.
    const char *__null_terminated null_result =
        convert_null(str, 3, __unsafe_forge_single(const char *, UINTPTR_MAX));
    printf("null_result: '%c'\n", *null_result);
    break;
  }
  default: {
    // Computing the pointer one past the terminator overflows.
    int *__terminated_by(-1) neg1_result = convert_neg1(
        buf, 3, __unsafe_forge_single(int *, UINTPTR_MAX - sizeof(int) + 1));
    printf("neg1_result: %d\n", *neg1_result);
    break;
  }
  }
  return 0;
}
