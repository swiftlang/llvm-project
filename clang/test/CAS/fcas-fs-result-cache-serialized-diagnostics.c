// RUN: rm -rf %t && mkdir -p %t
// RUN: llvm-cas --cas %t/cas --ingest --data %s > %t/casid

// RUN: %clang -cc1 -triple x86_64-apple-macos11 -fcas builtin \
// RUN:   -fcas-builtin-path %t/cas -fcas-fs @%t/casid -fcas-fs-result-cache \
// RUN:   -Wimplicit-function-declaration \
// RUN:   -Rcas-fs-result-cache-hit -emit-obj -o %t/output.o \
// RUN:   -serialize-diagnostic-file %t/diags %s 2>&1 \
// RUN:   | FileCheck %s --allow-empty --check-prefix=CACHE-MISS

// RUN: c-index-test -read-diagnostics %t/diags 2>&1 | FileCheck %s --check-prefix=SERIALIZED-MISS

// RUN: ls %t/output.o && rm %t/output.o

// RUN: %clang -cc1 -triple x86_64-apple-macos11 -fcas builtin \
// RUN:   -fcas-builtin-path %t/cas -fcas-fs @%t/casid -fcas-fs-result-cache \
// RUN:   -Wimplicit-function-declaration \
// RUN:   -Rcas-fs-result-cache-hit -emit-obj -o %t/output.o \
// RUN:   -serialize-diagnostic-file %t/diags %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=CACHE-HIT

// RUN: c-index-test -read-diagnostics %t/diags 2>&1 | FileCheck %s --check-prefix=SERIALIZED-HIT

// CACHE-HIT: remark: result cache hit
// CACHE-HIT: warning: implicit declaration

// CACHE-MISS: warning: implicit declaration
// CACHE-MISS-NOT: remark: result cache hit

// FIXME: serialized diagnostics should match the text diagnostics rdar://85234207
// SERIALIZED-HIT: warning: result cache hit for
// SERIALIZED-HIT: Number of diagnostics: 1
// SERIALIZED-MISS: Number of diagnostics: 0

void foo(void) {
  bar();
}
