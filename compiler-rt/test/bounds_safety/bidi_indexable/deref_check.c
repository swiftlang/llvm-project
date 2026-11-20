// Test the bounds checks performed when dereferencing a `__bidi_indexable`
// pointer.
//
// The address space overflow check is tested in `deref_overflow.c`.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
// RUN: %expect-trap --verify-prefix=below %s %t 1 2
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// above-merged{load}
// below-merged{load}
int __attribute__((noinline)) load(int *__bidi_indexable p) {
  // above@+2{dereferencing above upper bound in '*p'}
  // below@+1{dereferencing below lower bound in '*p'}
  return *p;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int storage[3] = {1, 2, 3};
  // Only `storage[1]` is within the bounds of `p`. The pointers dereferenced
  // below are within `storage` so in soft trap mode execution can continue
  // after the failed check.
  int *__bidi_indexable p =
      __unsafe_forge_bidi_indexable(int *, &storage[1], sizeof(int));
  switch (argc) {
  case 1:
    // In bounds: the checks are performed but don't fail.
    printf("%d\n", load(p));
    break;
  case 2:
    printf("%d\n", load(p + 1));
    break;
  default:
    printf("%d\n", load(p - 1));
    break;
  }
  return 0;
}
