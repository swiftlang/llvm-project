// RUN: rm -rf %t

// RUN: env LLVM_CAS_LOG=2 llvm-cas --cas %t/cas --ingest %s
// RUN: mv %t/cas/v1.1/v10.data %t/cas/v1.1/v10.data.bak

// RUN: env LLVM_CAS_LOG=2 %clang -cc1depscand -execute %{clang-daemon-dir}/%basename_t -cas-args -fcas-path %t/cas -- \
// RUN:   %clang -target x86_64-apple-macos11 -I %S/Inputs \
// RUN:     -Xclang -fcas-path -Xclang %t/cas \
// RUN:     -fdepscan=daemon -fdepscan-daemon=%{clang-daemon-dir}/%basename_t -fsyntax-only -x c %s

// RUN: ls %t/cas/corrupt.0.v1.1

// RUN: env LLVM_CAS_LOG=2 llvm-cas --cas %t/cas --validate-if-needed > %t/output

// Logging is enabled to try to catch a rare failure where validation is not
// skipped.
// RUN: cat %t/cas/v1.validation
// RUN: cat %t/cas/v1.log

// RUN: cat %t/output | FileCheck %s -check-prefix=SKIPPED
// SKIPPED: validation skipped

#include "test.h"

int func(void);
