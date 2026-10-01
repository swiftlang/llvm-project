// Time-trace settings are not part of the cache key: a second compile into
// another directory, with another trace path, granularity and verbosity, is a
// hit. The trace is written on a hit too.

// RUN: rm -rf %t && split-file %s %t && mkdir %t/miss %t/hit

// RUN: %clang -cc1depscan -fdepscan=inline -o %t/miss.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 \
// RUN:   -fcas-path %t/cas -fcache-compile-job -Rcompile-job-cache \
// RUN:   %t/src/main.c -emit-obj -o %t/miss/main.o \
// RUN:   -ftime-trace=%t/miss/main.json -ftime-trace-granularity=0
// RUN: %clang @%t/miss.rsp 2>&1 \
// RUN:   | FileCheck %s --allow-empty --check-prefix=CACHE-MISS
// RUN: ls %t/miss/main.json

// RUN: %clang -cc1depscan -fdepscan=inline -o %t/hit.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 \
// RUN:   -fcas-path %t/cas -fcache-compile-job -Rcompile-job-cache \
// RUN:   %t/src/main.c -emit-obj -o %t/hit/main.o \
// RUN:   -ftime-trace=%t/hit/main.json -ftime-trace-granularity=500 \
// RUN:   -ftime-trace-verbose
// RUN: %clang @%t/hit.rsp 2>&1 | FileCheck %s --check-prefix=CACHE-HIT
// RUN: ls %t/hit/main.o %t/hit/main.json

// CACHE-MISS-NOT: remark: compile job cache hit
// CACHE-HIT: remark: compile job cache hit

//--- src/main.c
int main(void) { return 0; }
