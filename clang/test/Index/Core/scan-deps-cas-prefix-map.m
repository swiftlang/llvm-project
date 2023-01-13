// RUN: rm -rf %t.mcp %t
// RUN: echo %S > %t.result
//
// RUN: c-index-test core --scan-deps %S -output-dir=%t \
// RUN:     -cas-path %t/cas -action-cache-path %t/cache \
// RUN:     -prefix-map-toolchain=/^tc -prefix-map-sdk=/^sdk \
// RUN:     -prefix-map=%S=/^src -prefix-map=%t=/^build \
// RUN:  -- %clang -c -I %S/Inputs/module \
// RUN:     -fmodules -fmodules-cache-path=%t.mcp \
// RUN:     -o FoE.o -x objective-c %s >> %t.result
// RUN: cat %t.result | sed 's/\\/\//g' | FileCheck %s -DOUTPUTS=%/t -DINPUTS=%/S

@import ModA;

// CHECK: [[PREFIX:.*]]
// CHECK-NEXT: modules:
// CHECK-NEXT:   module:
// CHECK-NEXT:     name: ModA
// CHECK-NEXT:     context-hash: [[HASH_MOD_A:[A-Z0-9]+]]
// CHECK-NEXT:     module-map-path: [[PREFIX]]/Inputs/module/module.modulemap
// CHECK-NEXT:     module-deps:
// CHECK-NEXT:     file-deps:
// CHECK-NEXT:       [[PREFIX]]/Inputs/module/ModA.h
// CHECK-NEXT:       [[PREFIX]]/Inputs/module/SubModA.h
// CHECK-NEXT:       [[PREFIX]]/Inputs/module/SubSubModA.h
// CHECK-NEXT:       [[PREFIX]]/Inputs/module/module.modulemap
// CHECK-NEXT:     build-args:
// CHECK-SAME:       -cc1
// CHECK-SAME:       -fcas-path
// CHECK-SAME:       -faction-cache-path
// CHECK-SAME:       -fcas-fs llvmcas://{{[[:xdigit:]]+}}
// CHECK-SAME:       -fcas-fs-working-directory /^src
// CHECK-SAME:       -x objective-c /^src/Inputs/module/module.modulemap
// CHECK-SAME:       -isysroot /^sdk
// CHECK-SAME:       -resource-dir /^tc/lib/clang/
// CHECK-SAME:       -I /^src/Inputs/module
// CHECK-NOT: [[PREFIX]]
// CHECK-NOT: [[INPUTS]]

// CHECK-NEXT: dependencies:
// CHECK-NEXT:   command 0:
// CHECK-NEXT:     context-hash: [[HASH_TU:[A-Z0-9]+]]
// CHECK-NEXT:     module-deps:
// CHECK-NEXT:       ModA:[[HASH_MOD_A]]
// CHECK-NEXT:     file-deps:
// CHECK-NEXT:       [[PREFIX]]/scan-deps-cas-prefix-map.m
// CHECK-NEXT:     build-args:
// CHECK-SAME:       -cc1
// CHECK-SAME:       -fcas-path
// CHECK-SAME:       -faction-cache-path
// CHECK-SAME:       -fcas-fs llvmcas://{{[[:xdigit:]]+}}
// CHECK-SAME:       -fcas-fs-working-directory /^src
// CHECK-SAME:       -fcache-compile-job
// CHECK-SAME:       -fmodule-file-cache-key=[[PCM:/\^build/ModA_.*pcm]]=llvmcas://{{[[:xdigit:]]+}}
// CHECK-SAME:       -x objective-c /^src/scan-deps-cas-prefix-map.m
// CHECK-SAME:       -isysroot /^sdk
// CHECK-SAME:       -resource-dir /^tc/lib/clang/
// CHECK-SAME:       -fmodule-file={{(ModA=)?}}[[PCM]]
// CHECK-SAME:       -I /^src/Inputs/module
// CHECK-NOT: [[PREFIX]]
// CHECK-NOT: [[INPUTS]]
