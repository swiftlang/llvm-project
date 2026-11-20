// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify=none %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe -verify=user %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-forges -verify=user %s

// Tests -Wbounds-safety-unsafe(-forges) for forges in user code that is needed
// to use SDK APIs. The forges inside the SDK header are not diagnosed here. See
// unsafe-forges-api-sdk.c for the SDK variants.

// none-no-diagnostics
#include <ptrcheck.h>
#include "unsafe-forges-sdk-api.h"

// Forge is necessary to get a safe pointer from the __unsafe_indexable pointer
// returned by the SDK.
void foo(void) {
  // user-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  int *local_int = __unsafe_forge_single(int *, getInt());
}

void print_version_terminated_by(void) {
  // user-warning@+2{{use of '__unsafe_forge_terminated_by' is unsafe}}
  const char *__terminated_by(1) version =
      __unsafe_forge_terminated_by(const char *, get_version(), 1);
}

void print_version_null_terminated(void) {
  // user-warning@+2{{use of '__unsafe_forge_null_terminated' is unsafe}}
  const char *__null_terminated version =
      __unsafe_forge_null_terminated(const char *, get_version());
}

void alloc_and_zero(void) {
  const size_t alloc_size = sizeof(int) * 2;
  // user-warning@+1{{use of '__unsafe_forge_bidi_indexable' is unsafe}}
  int *data = (int *)__unsafe_forge_bidi_indexable(
      void *, alloc(alloc_size), alloc_size);
  __builtin_memset(data, 0, alloc_size);
}

// The builtins are diagnosed the same way as the macros.
void builtins(void) {
  // user-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  int *s = (int *)__builtin_unsafe_forge_single(getInt());
  // user-warning@+1{{use of '__unsafe_forge_terminated_by' is unsafe}}
  const char *__terminated_by(1) t = __builtin_unsafe_forge_terminated_by(get_version(), 1);
  // user-warning@+1{{use of '__unsafe_forge_null_terminated' is unsafe}}
  const char *__null_terminated n = __builtin_unsafe_forge_terminated_by(get_version(), 0);
  // user-warning@+1{{use of '__unsafe_forge_bidi_indexable' is unsafe}}
  int *b = __builtin_unsafe_forge_bidi_indexable(getInt(), sizeof(int));
}
