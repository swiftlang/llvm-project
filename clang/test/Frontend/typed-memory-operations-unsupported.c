// The language mode comes from -x in every RUN below: the suffix list in
// clang/test/Frontend/lit.local.cfg has no .cu, so a .cu file is silently not
// discovered.
//
// RUN: not %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu -ftyped-memory-operations -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=CUDA
// RUN: not %clang_cc1 -x cuda -triple nvptx64-nvidia-cuda -fcuda-is-device -ftyped-memory-operations -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=CUDA
// RUN: not %clang_cc1 -x hip -triple x86_64-unknown-linux-gnu -ftyped-memory-operations -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=HIP
// RUN: not %clang_cc1 -x cl -triple spir64-unknown-unknown -ftyped-memory-operations -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=OPENCL
// RUN: not %clang_cc1 -x clcpp -triple spir64-unknown-unknown -ftyped-memory-operations -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=OPENCL
//
// The attribute argument is built while parsing, before the language option is
// consulted, so naming a rewrite target is rejected even with the flag off.
//
// RUN: not %clang_cc1 -x cl -triple spir64-unknown-unknown -fsyntax-only %s 2>&1 | FileCheck %s --check-prefix=NOFLAG
//
// RUN: %clang_cc1 -x cuda -triple x86_64-unknown-linux-gnu -fsyntax-only %s
// RUN: %clang_cc1 -x c++ -triple x86_64-unknown-linux-gnu -ftyped-memory-operations -fsyntax-only %s

// CUDA: error: unsupported option '-ftyped-memory-operations' for language mode 'CUDA'
// HIP: error: unsupported option '-ftyped-memory-operations' for language mode 'HIP'
// OPENCL: error: unsupported option '-ftyped-memory-operations' for language mode 'OpenCL'
// NOFLAG: error: taking address of function is not allowed

void *typed_malloc(__SIZE_TYPE__, unsigned long long);
void *tmo_malloc(__SIZE_TYPE__)
    __attribute__((typed_memory_operation(typed_malloc, 1)));
