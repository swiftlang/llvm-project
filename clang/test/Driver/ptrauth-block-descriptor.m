// RUN: %clang -target arm64e-apple-ios %s -mkernel -### 2>&1 | FileCheck %s --check-prefix=YES
// RUN: %clang -target arm64e-apple-ios %s -fapple-kext -### 2>&1 | FileCheck %s --check-prefix=YES
// RUN: %clang -target arm64e-apple-ios %s -fapple-kext -fptrauth-block-descriptor-pointers -### 2>&1 | FileCheck %s --check-prefix=YES
// RUN: %clang -target arm64e-apple-ios %s -### 2>&1 | FileCheck %s --check-prefix=NO
// RUN: %clang -target arm64e-apple-ios %s -mkernel -fno-ptrauth-block-descriptor-pointers -### 2>&1 | FileCheck %s --check-prefix=NO

// YES: "-cc1"{{.*}} "-fptrauth-block-descriptor-pointers"
// NO-NOT: "-cc1"{{.*}} "-fptrauth-block-descriptor-pointers"
