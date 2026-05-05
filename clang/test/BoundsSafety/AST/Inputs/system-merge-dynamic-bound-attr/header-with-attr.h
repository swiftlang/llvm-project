#include <ptrcheck.h>
void *myalloc(unsigned) __attribute__((alloc_size(1)));
void * __sized_by_or_null(size1) myalloc2(unsigned size1);
void *myalloc3(unsigned size1) __attribute__((alloc_size(1)));
void * __sized_by_or_null(size1) myalloc4(unsigned size1);
