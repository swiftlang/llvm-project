// -fdebug-prefix-map remaps a parent of the directory remapped for
// compilation caching. Without caching, the paths in the debug info start with
// /foo/src. With caching, the compiler only sees paths starting with /^src,
// which the prefix map for the parent directory doesn't apply to. Producing
// the same debug info requires composing the two prefix maps, which isn't
// supported yet.

// XFAIL: *

// RUN: rm -rf %t && mkdir -p %t/src
// RUN: cp %s %t/src/main.c

// RUN: %clang -cc1 -triple x86_64-apple-macos11 -emit-llvm \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src -fdebug-prefix-map=%t=/foo \
// RUN:   %t/src/main.c -o %t/plain.ll
// RUN: grep -E "DICompileUnit|DIFile" %t/plain.ll > %t/plain.txt

// RUN: %clang -cc1depscan -fdepscan=inline -o %t/cached.rsp -cc1-args \
// RUN:   -cc1 -triple x86_64-apple-macos11 -emit-llvm -fcas-path %t/cas \
// RUN:   -fdepscan-prefix-map %t/src %/root^src \
// RUN:   -debug-info-kind=standalone -dwarf-version=4 -main-file-name main.c \
// RUN:   -fdebug-compilation-dir=%t/src -fdebug-prefix-map=%t=/foo \
// RUN:   %t/src/main.c -o %t/cached.ll
// RUN: %clang @%t/cached.rsp
// RUN: grep -E "DICompileUnit|DIFile" %t/cached.ll > %t/cached.txt
// RUN: diff %t/plain.txt %t/cached.txt

int main(void) { return 0; }
