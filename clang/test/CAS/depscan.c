// RUN: %clang -cc1depscan -cc1-args -cc1 -triple x86_64-apple-macos11.0 -fcas builtin -fcas-builtin-path %t/cas-for-depscan-test -x c %s -o %s.o 2>&1 | FileCheck %s
// RUN: %clang -cc1depscan -cc1-args -triple x86_64-apple-macos11.0 -fcas builtin -fcas-builtin-path %t/cas-for-depscan-test -x c %s -o %s.o 2>&1 | FileCheck %s

// CHECK: "-fcas" "builtin"
// CHECK: "-fcas-builtin-path" "{{.*}}cas-for-depscan-test"

int test() { return 0; }
