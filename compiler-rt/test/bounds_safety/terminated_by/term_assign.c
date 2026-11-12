// Test the check that an assignment through a `__terminated_by` pointer
// doesn't overwrite its terminator.
//
// RUN: %clang_bsafe %s -o %t
// RUN: %expect-no-trap %t
// RUN: %expect-trap --verify-prefix=null-index %s %t 1
// RUN: %expect-trap --verify-prefix=null-deref %s %t 1 2
// RUN: %expect-trap --verify-prefix=null-compound %s %t 1 2 3
// RUN: %expect-trap --verify-prefix=neg1-index %s %t 1 2 3 4
// RUN: %expect-trap --verify-prefix=neg1-deref %s %t 1 2 3 4 5
#include "soft_trap_runtime_impl.h"
#include <ptrcheck.h>
#include <stdio.h>

// `noinline` here is used to prevent the checks from being optimized using the
// values passed by `main`. It also prevents inlining which can indirectly lead
// to LLVM figuring out UB is happening during soft traps and emit a hard trap
// after the soft trap call. This is a workaround for rdar://183581715

// null-index-merged{index_null}
void __attribute__((noinline)) index_null(char *__null_terminated p) {
  // null-index@+1{assigning to 'p[0]' overwrites the null terminator of 'char *__null_terminated' pointer 'p'}
  p[0] = 'x';
}

// null-deref-merged{deref_null}
void __attribute__((noinline)) deref_null(char *__null_terminated p) {
  // null-deref@+1{assigning to '*p' overwrites the null terminator of 'char *__null_terminated' pointer 'p'}
  *p = 'x';
}

// null-compound-merged{compound_null}
void __attribute__((noinline)) compound_null(char *__null_terminated p) {
  // null-compound@+1{assigning to '*p' overwrites the null terminator of 'char *__null_terminated' pointer 'p'}
  *p += 1;
}

// neg1-index-merged{index_neg1}
void __attribute__((noinline)) index_neg1(int *__terminated_by(-1) p) {
  // neg1-index@+1{assigning to 'p[0]' overwrites the terminator '-1' of 'int *__terminated_by(-1)' pointer 'p'}
  p[0] = 1;
}

// neg1-deref-merged{deref_neg1}
void __attribute__((noinline)) deref_neg1(int *__terminated_by(-1) p) {
  // neg1-deref@+1{assigning to '*p' overwrites the terminator '-1' of 'int *__terminated_by(-1)' pointer 'p'}
  *p = 1;
}

int main(int argc, const char **__counted_by(argc) argv) {
  char str[] = {'a', '\0'};
  int buf[] = {1, -1};
  char *__null_terminated str_start =
      __unsafe_null_terminated_from_indexable(str);
  int *__terminated_by(-1) buf_start =
      __unsafe_terminated_by_from_indexable(-1, buf);
  // These point at the terminator.
  char *__null_terminated str_term = str_start + 1;
  int *__terminated_by(-1) buf_term = buf_start + 1;
  // In soft trap mode execution continues after the failed check and the
  // terminator is overwritten so the arrays aren't read after that.
  switch (argc) {
  case 1:
    // The elements assigned to aren't the terminator: the checks are performed
    // but don't fail.
    index_null(str_start);
    deref_null(str_start);
    compound_null(str_start);
    index_neg1(buf_start);
    deref_neg1(buf_start);
    break;
  case 2:
    index_null(str_term);
    break;
  case 3:
    deref_null(str_term);
    break;
  case 4:
    compound_null(str_term);
    break;
  case 5:
    index_neg1(buf_term);
    break;
  default:
    deref_neg1(buf_term);
    break;
  }
  printf("done\n");
  return 0;
}
