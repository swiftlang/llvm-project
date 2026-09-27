// Swiftlang-specific: verify that on non-Darwin, when the Swift toolchain
// layout is present next to the clang resource dir, -fsanitize=thread
// auto-links the swift-corelibs-libdispatch and BlocksRuntime libraries that
// the TSan runtime (built with COMPILER_RT_INTERCEPT_LIBDISPATCH) depends on.
// When those files aren't shipped (upstream llvm builds), the driver must not
// reference them. TSan does not support -static, so only shared libs matter.

// Build a fake toolchain layout with placeholder tsan runtimes for both arches
// and shared libdispatch/BlocksRuntime alongside the resource dir.
//
// RUN: rm -rf %t && mkdir -p                                                 \
// RUN:   %t/lib/clang/00/lib/x86_64-unknown-linux-gnu                        \
// RUN:   %t/lib/clang/00/lib/aarch64-unknown-linux-gnu                       \
// RUN:   %t/lib/clang/00/lib/linux                                           \
// RUN:   %t/lib/swift/linux
// RUN: touch                                                                 \
// RUN:   %t/lib/clang/00/lib/x86_64-unknown-linux-gnu/libclang_rt.tsan.a     \
// RUN:   %t/lib/clang/00/lib/aarch64-unknown-linux-gnu/libclang_rt.tsan.a    \
// RUN:   %t/lib/clang/00/lib/linux/libclang_rt.tsan-x86_64.a                 \
// RUN:   %t/lib/clang/00/lib/linux/libclang_rt.tsan-aarch64.a                \
// RUN:   %t/lib/swift/linux/libBlocksRuntime.so                              \
// RUN:   %t/lib/swift/linux/libdispatch.so

// DEFINE: %{clang} = %clang -### -fsanitize=thread -fuse-ld=ld              \
// DEFINE:              -resource-dir=%t/lib/clang/00 %s 2>&1

// Shared libdispatch/BlocksRuntime + rpath into swift/linux.
// RUN: %{clang} --target=x86_64-unknown-linux-gnu | FileCheck --check-prefix=SHARED %s
// RUN: %{clang} --target=aarch64-unknown-linux-gnu | FileCheck --check-prefix=SHARED %s
// SHARED: "{{.*}}swift/linux/libBlocksRuntime.so"
// SHARED-SAME: "{{.*}}swift/linux/libdispatch.so"
// SHARED-SAME: "-rpath" "{{.*}}swift/linux"

// When the Swift resource dir isn't present (upstream llvm layout), the driver
// must not reference either library or rpath.
// RUN: rm -rf %t/lib/swift
// RUN: %{clang} --target=x86_64-unknown-linux-gnu \
// RUN:   | FileCheck --check-prefix=NONE %s
// NONE-NOT: libBlocksRuntime.{{(so|a)}}
// NONE-NOT: libdispatch.{{(so|a)}}
// NONE-NOT: "-rpath" "{{.*}}swift/linux"

int main(void) { return 0; }
