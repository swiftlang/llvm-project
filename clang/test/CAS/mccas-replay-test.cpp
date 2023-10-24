// RUN: rm -rf %t && mkdir -p %t
// RUN: export LLVM_CACHE_CAS_PATH=%t/cas && %clang-cache \
// RUN:   %clang -target x86_64-apple-macos11 -c -Xclang -fcas-backend -Rcompile-job-cache %s -o %t/tmp.o 2>&1 | FileCheck %s -check-prefix=CACHE-MISS
// CACHE-MISS: remark: compile job cache miss

// RUN: wc -c %t/tmp.o | FileCheck %s -check-prefix=CACHE-MISS-FILE
// CACHE-MISS-FILE-NOT: 0 %t/tmp.o 

// RUN: export LLVM_CACHE_CAS_PATH=%t/cas && %clang-cache \
// RUN:   %clang -target x86_64-apple-macos11 -c -Xclang -fcas-backend -Rcompile-job-cache %s -o %t/tmp.o 2>&1 | FileCheck %s -check-prefix=CACHE-HIT
// CACHE-HIT: remark: compile job cache hit

// RUN: wc -c %t/tmp.o | FileCheck %s -check-prefix=CACHE-HIT-FILE
// CACHE-HIT-FILE-NOT: 0 %t/tmp.o 


int foo() {
    return 1;
}