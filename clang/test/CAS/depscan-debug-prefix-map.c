// Test that -fdebug-prefix-map produces the same debug info with and without
// prefix mapping for compilation caching, when it remaps the same directory as
// the caching prefix map, or a directory inside it.

// RUN: rm -rf %t && mkdir -p %t/src %t/src2
// RUN: cp %s %t/src/main.c
// RUN: cp %s %t/src2/main.c

// RUN: %clang -cc1 -triple x86_64-apple-macos11 -emit-llvm \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src -fdebug-prefix-map=%t/src=/foo \
// RUN:   %t/src/main.c -o %t/plain.ll
// RUN: FileCheck %s -input-file %t/plain.ll
// RUN: grep -E "DICompileUnit|DIFile" %t/plain.ll > %t/plain.txt

// CHECK: !DIFile(filename: "main.c", directory: "/foo")

// Both prefix maps remap the same directory.
// RUN: %clang -cc1depscan -fdepscan=inline -o %t/same.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -emit-llvm -fcas-path %t/cas \
// RUN:   -Rcompile-job-cache -fdepscan-prefix-map %t/src %/root^src \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src -fdebug-prefix-map=%t/src=/foo \
// RUN:   %t/src/main.c -o %t/same.ll
// RUN: %clang @%t/same.rsp 2>&1 | FileCheck %s --check-prefix=CACHE-MISS
// RUN: grep -E "DICompileUnit|DIFile" %t/same.ll > %t/same.txt
// RUN: diff %t/plain.txt %t/same.txt

// CACHE-MISS: remark: compile job cache miss

// The remapped -fdebug-prefix-map doesn't depend on the source directory, so
// compiling the same file in another directory is a cache hit.
// RUN: %clang -cc1depscan -fdepscan=inline -o %t/same2.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -emit-llvm -fcas-path %t/cas \
// RUN:   -Rcompile-job-cache -fdepscan-prefix-map %t/src2 %/root^src \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src2 -fdebug-prefix-map=%t/src2=/foo \
// RUN:   %t/src2/main.c -o %t/same2.ll
// RUN: %clang @%t/same2.rsp 2>&1 | FileCheck %s --check-prefix=CACHE-HIT
// RUN: grep -E "DICompileUnit|DIFile" %t/same2.ll > %t/same2.txt
// RUN: diff %t/plain.txt %t/same2.txt

// CACHE-HIT: remark: compile job cache hit

// -fdebug-prefix-map remaps a directory inside the one remapped for caching.
// RUN: %clang -cc1depscan -fdepscan=inline -o %t/inside.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -emit-llvm -fcas-path %t/cas \
// RUN:   -fdepscan-prefix-map %t %/root^t \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src -fdebug-prefix-map=%t/src=/foo \
// RUN:   %t/src/main.c -o %t/inside.ll
// RUN: %clang @%t/inside.rsp
// RUN: grep -E "DICompileUnit|DIFile" %t/inside.ll > %t/inside.txt
// RUN: diff %t/plain.txt %t/inside.txt

struct S {
  int x;
};

int main(void) {
  struct S s = {0};
  return s.x;
}
