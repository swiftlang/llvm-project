// Test the bounds checks performed when converting a `__bidi_indexable` pointer
// to a `__single` pointer.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
// RUN: %expect-trap --verify-prefix=overflow %s %t 1 2
// RUN: %expect-trap --verify-prefix=below %s %t 1 2 3
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
// below-merged{to_single}
int *__single __attribute__((noinline)) to_single(int *__bidi_indexable p) {
  // above@+3{converting 'int *__bidi_indexable' to 'int *__single' where pointer 'p' is above its upper bound}
  // overflow@+2{converting 'int *__bidi_indexable' to 'int *__single' where pointer 'p' overflows the address space}
  // below@+1{converting 'int *__bidi_indexable' to 'int *__single' where pointer 'p' is below its lower bound}
  return p;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int storage[3] = {1, 2, 3};
  // Only `storage[1]` is within the bounds of `p`. In soft trap mode
  // execution continues after the failed check, so the converted pointers
  // aren't dereferenced in the cases where the check fails.
  int *__bidi_indexable p =
      __unsafe_forge_bidi_indexable(int *, &storage[1], sizeof(int));
  switch (argc) {
  case 1:
    // In bounds: the checks are performed but don't fail.
    printf("%d\n", *to_single(p));
    break;
  case 2:
    to_single(p + 1);
    break;
  case 3:
    to_single(__unsafe_forge_bidi_indexable(
        int *, UINTPTR_MAX - sizeof(int) + 1, sizeof(int)));
    break;
  default:
    to_single(p - 1);
    break;
  }
  printf("done\n");
  return 0;
}
