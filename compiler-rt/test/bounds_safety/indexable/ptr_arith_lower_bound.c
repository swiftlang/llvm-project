// Test the check that pointer arithmetic on an `__indexable` pointer doesn't
// move it below its lower bound (which is the pointer itself).
//
// Only pointer arithmetic whose result is still `__indexable` (compound
// assignment and increment) is checked here. The result of other pointer
// arithmetic (e.g. `p + n`) is `__bidi_indexable`.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=add-below %s %t 1
// RUN: %expect-trap --verify-prefix=sub-below %s %t 1 2
// RUN: %expect-trap --verify-prefix=inc-overflow %s %t 1 2 3
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// add-below-merged{add}
int *__indexable __attribute__((noinline)) add(int *__indexable i, long n) {
  // add-below@+1{pointer arithmetic 'i += n' moves 'int *__indexable' pointer 'i' below its lower bound}
  i += n;
  return i;
}

// sub-below-merged{sub}
int *__indexable __attribute__((noinline)) sub(int *__indexable i, long n) {
  // sub-below@+1{pointer arithmetic 'i -= n' moves 'int *__indexable' pointer 'i' below its lower bound}
  i -= n;
  return i;
}

// inc-overflow-merged{inc}
int *__indexable __attribute__((noinline)) inc(int *__indexable i) {
  // inc-overflow@+1{pointer arithmetic '++i' moves 'int *__indexable' pointer 'i' below its lower bound}
  return ++i;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int arr[] = {1, 2, 3};
  // Converting `&arr[1]` to `__indexable` drops its lower bound (`&arr[0]`) so
  // the bounds of `mid` are `&arr[1]` to `&arr[3]`. Moving `mid` backwards
  // fails the check even though `arr[0]` exists.
  int *__indexable mid = &arr[1];
  // In soft trap mode execution continues after the failed check so the
  // results aren't dereferenced in that case.
  switch (argc) {
  case 1:
    // The pointers move forward: the checks are performed but don't fail.
    printf("%d %d %d\n", *add(mid, 1), *sub(mid, -1), *inc(mid));
    break;
  case 2:
    add(mid, -1);
    break;
  case 3:
    sub(mid, 1);
    break;
  default:
    // Incrementing the pointer at the end of the address space wraps around.
    inc(__unsafe_forge_bidi_indexable(int *, UINTPTR_MAX - sizeof(int) + 1,
                                      sizeof(int)));
    break;
  }
  printf("done\n");
  return 0;
}
