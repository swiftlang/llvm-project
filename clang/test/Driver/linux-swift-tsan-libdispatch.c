// Swiftlang-specific: verify that on Linux, when the Swift toolchain layout is
// present next to the clang resource dir, -fsanitize=thread auto-links the
// swift-corelibs-libdispatch and BlocksRuntime libraries that the TSan runtime
// (built with COMPILER_RT_INTERCEPT_LIBDISPATCH) depends on. When those files
// are not shipped (upstream llvm builds), the driver must not add them.

// Build a fake toolchain layout with placeholder tsan runtimes for both arches
// and both dynamic and static libdispatch/BlocksRuntime.
//
// RUN: rm -rf %t && mkdir -p                                                 \
// RUN:   %t/lib/clang/00/lib/x86_64-unknown-linux-gnu                        \
// RUN:   %t/lib/clang/00/lib/aarch64-unknown-linux-gnu                       \
// RUN:   %t/lib/clang/00/lib/linux                                           \
// RUN:   %t/lib/swift/linux %t/lib/swift_static/linux
// RUN: touch                                                                 \
// RUN:   %t/lib/clang/00/lib/x86_64-unknown-linux-gnu/libclang_rt.tsan.a     \
// RUN:   %t/lib/clang/00/lib/aarch64-unknown-linux-gnu/libclang_rt.tsan.a    \
// RUN:   %t/lib/clang/00/lib/linux/libclang_rt.tsan-x86_64.a                 \
// RUN:   %t/lib/clang/00/lib/linux/libclang_rt.tsan-aarch64.a                \
// RUN:   %t/lib/swift/linux/libBlocksRuntime.so                              \
// RUN:   %t/lib/swift/linux/libdispatch.so                                   \
// RUN:   %t/lib/swift_static/linux/libBlocksRuntime.a                        \
// RUN:   %t/lib/swift_static/linux/libdispatch.a

// DEFINE: %{clang} = %clang -### -fsanitize=thread -fuse-ld=ld              \
// DEFINE:              -resource-dir=%t/lib/clang/00 %s 2>&1

// Dynamic link: shared libdispatch/BlocksRuntime + rpath into swift/linux.
// RUN: %{clang} --target=x86_64-unknown-linux-gnu | FileCheck --check-prefix=SHARED %s
// RUN: %{clang} --target=aarch64-unknown-linux-gnu | FileCheck --check-prefix=SHARED %s
// SHARED: "{{.*}}swift/linux/libBlocksRuntime.so"
// SHARED-SAME: "{{.*}}swift/linux/libdispatch.so"
// SHARED-SAME: "-rpath" "{{.*}}swift/linux"

// Static link: .a variants from swift_static/linux, no rpath.
// RUN: %{clang} --target=x86_64-unknown-linux-gnu -static \
// RUN:   | FileCheck --check-prefix=STATIC %s
// RUN: %{clang} --target=aarch64-unknown-linux-gnu -static-pie \
// RUN:   | FileCheck --check-prefix=STATIC %s
// STATIC: "{{.*}}swift_static/linux/libBlocksRuntime.a"
// STATIC-SAME: "{{.*}}swift_static/linux/libdispatch.a"
// STATIC-NOT: "-rpath" "{{.*}}swift{{(_static)?}}/linux"

// When the Swift resource dir isn't present (upstream llvm layout), the driver
// must not reference either library or rpath.
// RUN: rm -rf %t/lib/swift %t/lib/swift_static
// RUN: %{clang} --target=x86_64-unknown-linux-gnu \
// RUN:   | FileCheck --check-prefix=NONE %s
// NONE-NOT: libBlocksRuntime.{{(so|a)}}
// NONE-NOT: libdispatch.{{(so|a)}}
// NONE-NOT: "-rpath" "{{.*}}swift{{(_static)?}}/linux"

// Android must never get these deps (its TSan build has no libdispatch
// interceptors, and even if the files were present, they aren't for bionic).
// RUN: mkdir -p %t/lib/swift/linux && touch                                  \
// RUN:   %t/lib/swift/linux/libBlocksRuntime.so                              \
// RUN:   %t/lib/swift/linux/libdispatch.so
// RUN: %{clang} --target=aarch64-linux-android \
// RUN:   | FileCheck --check-prefix=NONE %s

int main(void) { return 0; }
