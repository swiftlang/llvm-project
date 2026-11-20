#pragma clang system_header
#include <ptrcheck.h>
#include <stddef.h>

// SDK declarations whose pointers are __unsafe_indexable by default.
int * /*__unsafe_indexable*/ getInt(void);
const char * /*__unsafe_indexable*/ get_version(void);
void * /*__unsafe_indexable*/ alloc(size_t size);

// Inline functions using each forge kind, to check which warnings fire for
// forges inside the SDK header.
static inline int *sdk_api_forge_single(int *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  return __unsafe_forge_single(int *, p);
}

static inline char *__terminated_by(1) sdk_api_forge_terminated_by(char *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_terminated_by' is unsafe}}
  return __unsafe_forge_terminated_by(char *, p, 1);
}

static inline char *__null_terminated sdk_api_forge_null_terminated(char *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_null_terminated' is unsafe}}
  return __unsafe_forge_null_terminated(char *, p);
}

static inline void *sdk_api_forge_bidi_indexable(void *p, unsigned n) {
  // sdk-warning@+1{{use of '__unsafe_forge_bidi_indexable' is unsafe}}
  return __unsafe_forge_bidi_indexable(void *, p, n);
}

static inline int *sdk_api_builtin_forge_single(int *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_single' is unsafe}}
  return (int *)__builtin_unsafe_forge_single(p);
}

static inline char *__terminated_by(1)
sdk_api_builtin_forge_terminated_by(char *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_terminated_by' is unsafe}}
  return __builtin_unsafe_forge_terminated_by(p, 1);
}

static inline char *__null_terminated
sdk_api_builtin_forge_null_terminated(char *p) {
  // sdk-warning@+1{{use of '__unsafe_forge_null_terminated' is unsafe}}
  return __builtin_unsafe_forge_terminated_by(p, 0);
}

static inline void *sdk_api_builtin_forge_bidi_indexable(void *p, unsigned n) {
  // sdk-warning@+1{{use of '__unsafe_forge_bidi_indexable' is unsafe}}
  return __builtin_unsafe_forge_bidi_indexable(p, n);
}
