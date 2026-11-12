// Test the check that a `__terminated_by` pointer isn't incremented past its
// terminator.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-unary %s %t 1
// RUN: %expect-trap --verify-prefix=null-binary %s %t 1 2
// RUN: %expect-trap --verify-prefix=neg1-unary %s %t 1 2 3
// RUN: %expect-trap --verify-prefix=neg1-binary %s %t 1 2 3 4
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// null-unary-merged{inc_null}
const char *__null_terminated __attribute__((noinline))
inc_null(const char *__null_terminated p) {
  // null-unary@+1{incrementing 'const char *__null_terminated' pointer past the null terminator in '++p'}
  return ++p;
}

// null-binary-merged{add_null}
const char *__null_terminated __attribute__((noinline))
add_null(const char *__null_terminated p) {
  // null-binary@+1{incrementing 'const char *__null_terminated' pointer past the null terminator in 'p + 1'}
  return p + 1;
}

// neg1-unary-merged{inc_neg1}
int *__terminated_by(-1) __attribute__((noinline))
inc_neg1(int *__terminated_by(-1) p) {
  // neg1-unary@+1{incrementing 'int *__terminated_by(-1)' pointer past the terminator '-1' in '++p'}
  return ++p;
}

// neg1-binary-merged{add_neg1}
int *__terminated_by(-1) __attribute__((noinline))
add_neg1(int *__terminated_by(-1) p) {
  // neg1-binary@+1{incrementing 'int *__terminated_by(-1)' pointer past the terminator '-1' in 'p + 1'}
  return p + 1;
}

int main(int argc, const char **__counted_by(argc) argv) {
  const char *__null_terminated str = "a";
  int buf[] = {1, -1};
  int *__terminated_by(-1) neg1 =
      __unsafe_terminated_by_from_indexable(-1, buf);
  // In soft trap mode execution continues after the failed check. The pointers
  // returned in that case are past the terminator so they aren't dereferenced.
  switch (argc) {
  case 1:
    // The pointers aren't at the terminator: the checks are performed but
    // don't fail.
    printf("%c %c %d %d\n", *inc_null(str), *add_null(str), *inc_neg1(neg1),
           *add_neg1(neg1));
    break;
  case 2:
    // The pointers are at the terminator.
    inc_null(inc_null(str));
    break;
  case 3:
    add_null(add_null(str));
    break;
  case 4:
    inc_neg1(inc_neg1(neg1));
    break;
  default:
    add_neg1(add_neg1(neg1));
    break;
  }
  printf("done\n");
  return 0;
}
