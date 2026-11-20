// Test the bounds checks performed when indexing the flexible array member of
// a struct through a `__single` pointer. The bounds come from the count of the
// flexible array member.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=above %s %t 1
// RUN: %expect-trap --verify-prefix=below %s %t 1 2
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

struct flexible {
  int count;
  int elems[__counted_by(count)];
};

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// above-merged{get}
// below-merged{get}
int __attribute__((noinline)) get(struct flexible *__single flex, int idx) {
  // above@+2{indexing above upper bound in 'flex->elems[idx]'}
  // below@+1{indexing below lower bound in 'flex->elems[idx]'}
  return flex->elems[idx];
}

int main(int argc, const char **__counted_by(argc) argv) {
  // The storage has room for 3 elements but the count is 2, so only the first
  // 2 elements are within the bounds of `flex->elems`. All the indices used
  // below are within the storage so in soft trap mode execution can continue
  // after the failed check.
  int storage[4] = {0, 10, 20, 30};
  struct flexible *__bidi_indexable wide = __unsafe_forge_bidi_indexable(
      struct flexible *, storage, sizeof(storage));
  wide->count = 2;
  struct flexible *__single flex = wide;
  switch (argc) {
  case 1:
    // The indices are within the count: the checks are performed but don't
    // fail.
    printf("%d %d\n", get(flex, 0), get(flex, 1));
    break;
  case 2:
    // The index is the count.
    printf("%d\n", get(flex, 2));
    break;
  default:
    printf("%d\n", get(flex, -1));
    break;
  }
  return 0;
}
