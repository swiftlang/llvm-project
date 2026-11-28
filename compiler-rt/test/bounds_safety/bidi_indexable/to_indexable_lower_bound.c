// Test the lower bound check performed when converting a `__bidi_indexable`
// pointer to an `__indexable` pointer.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=below %s %t 1
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the check from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// below-merged{convert}
int *__indexable __attribute__((noinline)) convert(int *__bidi_indexable p) {
  // below@+1{converting 'int *__bidi_indexable' to 'int *__indexable' where pointer 'p' is below its lower bound}
  return p;
}

int main(int argc, const char **__counted_by(argc) argv) {
  int arr[] = {1, 2};
  int *__bidi_indexable p = arr;
  if (argc == 1) {
    // In bounds and null pointers: the check is performed but doesn't fail.
    int *__indexable in_bounds = convert(&arr[1]);
    int *__indexable null = convert(0);
    printf("in_bounds: %d null: %d\n", *in_bounds, null == 0);
  } else {
    // The pointer is below its lower bound. In soft trap mode execution
    // continues after the failed check so the result isn't dereferenced.
    convert(p - 1);
    printf("done\n");
  }
  return 0;
}
