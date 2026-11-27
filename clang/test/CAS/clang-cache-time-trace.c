// Check CLANG_CACHE_TIME_TRACE in clang-cache:
// - One trace per compile is written next to the output.
// - CLANG_CACHE_TIME_TRACE_GRANULARITY that is not a number
//   produces an error.

// REQUIRES: ondisk_cas

// RUN: rm -rf %t && mkdir -p %t/miss %t/hit

// RUN: env LLVM_CACHE_CAS_PATH=%t/cas CLANG_CACHE_TIME_TRACE=1 \
// RUN:   CLANG_CACHE_TIME_TRACE_GRANULARITY=0 %clang-cache %clang \
// RUN:   -c %s -o %t/miss/main.o -Xclang -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-MISS
// RUN: FileCheck %s --check-prefix=TRACE --input-file=%t/miss/main.json

// RUN: env LLVM_CACHE_CAS_PATH=%t/cas CLANG_CACHE_TIME_TRACE=1 \
// RUN:   CLANG_CACHE_TIME_TRACE_GRANULARITY=0 %clang-cache %clang \
// RUN:   -c %s -o %t/hit/main.o -Xclang -Rcompile-job-cache 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-HIT
// RUN: FileCheck %s --check-prefixes=TRACE,TRACE-HIT \
// RUN:   --input-file=%t/hit/main.json

// RUN: not env LLVM_CACHE_CAS_PATH=%t/cas CLANG_CACHE_TIME_TRACE=1 \
// RUN:   CLANG_CACHE_TIME_TRACE_GRANULARITY=abc %clang-cache %clang \
// RUN:   -c %s -o %t/bad.o 2>&1 | FileCheck %s --check-prefix=BAD

// CACHE-MISS: remark: compile job cache miss
// CACHE-HIT: remark: compile job cache hit

// TRACE-DAG: "name":"shouldCacheInvocation"
// TRACE-DAG: "name":"Driver::BuildCompilation"
// TRACE-DAG: "name":"cc1depscan_main"
// TRACE-DAG: "name":"Backend Key Query"
// TRACE-HIT-DAG: "name":"Backend Load Artifacts"

// BAD: error: environment variable CLANG_CACHE_TIME_TRACE_GRANULARITY has invalid value abc

int main(void) { return 0; }
