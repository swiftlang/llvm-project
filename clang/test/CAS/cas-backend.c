// RUN: rm -rf %t && mkdir -p %t
//
// RUN: %clang -cc1 \
// RUN:   -triple x86_64-apple-macos11 -fcas-backend -fcas-backend-mode=casid \
// RUN:   -fcas-path %t/cas %s \
// RUN:   -emit-obj -o %t/output.o.casid \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -debugger-tuning=lldb
//
// RUN: llvm-cas-dump --cas %t/cas --casid-file %t/output.o.casid

// RUN: rm -rf %t && mkdir -p %t

// Also test that native mode creates a casid file.
// RUN: %clang -cc1 \
// RUN:   -triple x86_64-apple-macos11 -fcas-backend -fcas-backend-mode=native \
// RUN:   -fcas-path %t/cas %s \
// RUN:   -emit-obj -o %t/output.o \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -debugger-tuning=lldb
//
// RUN: llvm-cas-dump --cas %t/cas --casid-file %t/output.o.casid

void test(void) {}

int test1(void) {
  return 0;
}
