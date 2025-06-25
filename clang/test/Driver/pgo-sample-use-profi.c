/// Test if profi flag is enabled in frontend as user-facing feature.
// RUN: %clang --target=x86_64 -c -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=x86_64 -c -fsample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=x86_64 -c -fno-sample-profile-use-profi -fsample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=x86_64 -c -fno-sample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s --check-prefixes=CHECK-NO-PROFI
// RUN: %clang --target=x86_64 -c -fsample-profile-use-profi -fno-sample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s --check-prefixes=CHECK-NO-PROFI

// RUN: %clang --target=AArch64 -c -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=AArch64 -c -fsample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=AArch64 -c -fno-sample-profile-use-profi -fsample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s
// RUN: %clang --target=AArch64 -c -fno-sample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s --check-prefixes=CHECK-NO-PROFI
// RUN: %clang --target=AArch64 -c -fsample-profile-use-profi -fno-sample-profile-use-profi -fprofile-sample-use=/dev/null -### %s 2>&1 | FileCheck %s --check-prefixes=CHECK-NO-PROFI

// CHECK: "-mllvm" "-sample-profile-use-profi"
// CHECK-NO-PROFI-NOT: "-sample-profile-use-profi"
