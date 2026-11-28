// Test the bounds checks performed when converting an `__indexable` pointer to
// a `__single` pointer. An `__indexable` pointer doesn't have a lower bound so
// there is no lower bound check.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
// RUN: %expect-trap --verify-prefix=overflow %s %t 1 2
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdint.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// above-merged{to_single}
// overflow-merged{to_single}
int *__single __attribute__((noinline)) to_single(int *__indexable p) {
  // above@+2{converting 'int *__indexable' to 'int *__single' where pointer 'p' is above its upper bound}
  // overflow@+1{converting 'int *__indexable' to 'int *__single' where pointer 'p' overflows the address space}
  return p;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int storage[3] = {1, 2, 3};
  // Only `storage[1]` is within the bounds of `p`. In soft trap mode
  // execution continues after the failed check, so the converted pointers
  // aren't dereferenced in the cases where the check fails.
  int *__indexable p =
      __unsafe_forge_bidi_indexable(int *, &storage[1], sizeof(int));
  switch (argc) {
  case 1:
    // In bounds: the checks are performed but don't fail.
    printf("%d\n", *to_single(p));
    break;
  case 2:
    to_single(p + 1);
    break;
  default:
    to_single(__unsafe_forge_bidi_indexable(
        int *, UINTPTR_MAX - sizeof(int) + 1, sizeof(int)));
    break;
  }
  printf("done\n");
  return 0;
}
