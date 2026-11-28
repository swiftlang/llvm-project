// Test the bounds checks performed when accessing a member through an
// `__indexable` pointer. An `__indexable` pointer doesn't have a lower bound so
// there is no lower bound check.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

struct S {
  int a;
  int b;
};

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// above-merged{load}
int __attribute__((noinline)) load(struct S *__indexable p) {
  // above@+1{member access above upper bound in 'p->b'}
  return p->b;
}

int main(int argc, const char **__counted_by(argc) argv) {
  struct S storage[3] = {{1, 2}, {3, 4}, {5, 6}};
  // Only `storage[1]` is within the bounds of `p`. The out of bounds members
  // accessed below are within `storage` so in soft trap mode execution can
  // continue after the failed check.
  struct S *__indexable p =
      __unsafe_forge_bidi_indexable(struct S *, &storage[1], sizeof(struct S));
  switch (argc) {
  case 1:
    // In bounds: the checks are performed but don't fail.
    printf("%d\n", load(p));
    break;
  default:
    printf("%d\n", load(p + 1));
    break;
  }
  return 0;
}
