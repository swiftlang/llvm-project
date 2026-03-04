// REQUIRES: ondisk_cas
// REQUIRES: system-windows

// RUN: rm -rf %t
// RUN: split-file %s %t

// RUN: env LLVM_CACHE_CAS_PATH=%t/cas %clang-cache %clang_cl /Yc%t/cmake_pch.hxx /Fp%t/cmake_pch.pch /FI%t/cmake_pch.hxx /Fo%t/cmake_pch.obj /Fd%t -Xclang -Rcompile-job-cache -c -- %t/cmake_pch.cxx 2>&1 | FileCheck %s -check-prefix=CACHE-MISS
// RUN: env LLVM_CACHE_CAS_PATH=%t/cas %clang-cache %clang_cl /Yu%t/cmake_pch.hxx /Fp%t/cmake_pch.pch /FI%t/cmake_pch.hxx /Fo%t/Test.obj /Fd%t -Xclang -Rcompile-job-cache -c -- %t/Test.cpp 2>&1 | FileCheck %s -check-prefix=CACHE-MISS

// RUN: env LLVM_CACHE_CAS_PATH=%t/cas %clang-cache %clang_cl /Yc%t/cmake_pch.hxx /Fp%t/cmake_pch.pch /FI%t/cmake_pch.hxx /Fo%t/cmake_pch.obj /Fd%t -Xclang -Rcompile-job-cache -c -- %t/cmake_pch.cxx 2>&1 | FileCheck %s -check-prefix=CACHE-HIT
// RUN: env LLVM_CACHE_CAS_PATH=%t/cas %clang-cache %clang_cl /Yu%t/cmake_pch.hxx /Fp%t/cmake_pch.pch /FI%t/cmake_pch.hxx /Fo%t/Test.obj /Fd%t -Xclang -Rcompile-job-cache -c -- %t/Test.cpp 2>&1 | FileCheck %s -check-prefix=CACHE-HIT

// CACHE-HIT: remark: compile job cache hit
// CACHE-MISS: remark: compile job cache miss

//--- Test.cpp
#include "pch.h"

int foo() {
  return 42;
}

//--- pch.h
#pragma once

//--- cmake_pch.cxx
// empty

//--- cmake_pch.hxx
#pragma clang system_header
#include "pch.h"
