// Check that we can manually enable specific ptrauth features.

// RUN: %clang -target arm64-apple-ios15 -c %s -### 2>&1 | FileCheck %s --check-prefix NONE
// NONE: "-cc1"
// NONE-NOT: "-fptrauth-intrinsics"
// NONE-NOT: "-fptrauth-calls"
// NONE-NOT: "-fptrauth-returns"
// NONE-NOT: "-fptrauth-indirect-gotos"
// NONE-NOT: "-fptrauth-auth-traps"
// NONE-NOT: "-mbranch-target-enforce"
// NONE-NOT: "-fptrauth-soft"

// RUN: %clang -target arm64-apple-ios15 -fptrauth-calls -c %s -### 2>&1 | FileCheck %s --check-prefix CALL
// CALL: "-cc1"{{.*}} {{.*}} "-fptrauth-calls"

// RUN: %clang -target arm64-apple-ios15 -fptrauth-intrinsics -c %s -### 2>&1 | FileCheck %s --check-prefix INTRIN
// INTRIN: "-cc1"{{.*}} {{.*}} "-fptrauth-intrinsics"

// RUN: %clang -target arm64-apple-ios15 -fptrauth-returns -c %s -### 2>&1 | FileCheck %s --check-prefix RETURN
// RETURN: "-cc1"{{.*}} {{.*}} "-fptrauth-returns"

// RUN: %clang -target arm64-apple-ios15 -fptrauth-indirect-gotos -c %s -### 2>&1 | FileCheck %s --check-prefix INDGOTO
// INDGOTO: "-cc1"{{.*}} {{.*}} "-fptrauth-indirect-gotos"

// RUN: %clang -target arm64-apple-ios15 -fptrauth-auth-traps -c %s -### 2>&1 | FileCheck %s --check-prefix TRAPS
// TRAPS: "-cc1"{{.*}} {{.*}} "-fptrauth-auth-traps"

// RUN: %clang -target arm64-apple-ios15 -fbranch-target-identification -c %s -### 2>&1 | FileCheck %s --check-prefix BTI
// BTI: "-cc1"{{.*}} {{.*}} "-mbranch-target-enforce"

// RUN: %clang -target arm64-apple-ios -fptrauth-soft -c %s -### 2>&1 | FileCheck %s --check-prefix SOFT
// SOFT: "-cc1"{{.*}} {{.*}} "-fptrauth-soft"


// Check the arm64e defaults.
// isa signing depends on the target OS and is tested elsewhere.

// RUN: %clang -target arm64e-apple-ios15 -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT
// RUN: %clang -mkernel -target arm64e-apple-ios15 -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT-KERN
// RUN: %clang -fapple-kext -target arm64e-apple-ios15 -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT-KERN
// DEFAULT: "-fptrauth-returns" "-fptrauth-intrinsics" "-fptrauth-calls" "-fptrauth-indirect-gotos" "-fptrauth-auth-traps" "-fno-assume-unique-vtables" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-objc-isa-mode=sign-and-auth" "-target-cpu" "apple-a12"{{.*}}
// DEFAULT-KERN: "-fptrauth-returns" "-fptrauth-intrinsics" "-fptrauth-calls" "-fptrauth-indirect-gotos" "-fptrauth-auth-traps" "-fno-assume-unique-vtables" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-objc-isa-mode=sign-and-auth" "-fptrauth-block-descriptor-pointers" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-function-pointer-type-discrimination" "-mbranch-target-enforce" "-target-cpu" "apple-a12"{{.*}}

// RUN: %clang -target arm64e-apple-ios15 -fno-ptrauth-calls -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT-NOCALL
// RUN: %clang -mkernel -target arm64e-apple-ios15 -fno-ptrauth-calls -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT-KERN-NOCALL
// RUN: %clang -fapple-kext -target arm64e-apple-ios15 -fno-ptrauth-calls -c %s -### 2>&1 | FileCheck %s --check-prefix DEFAULT-KERN-NOCALL
// DEFAULT-NOCALL-NOT: "-fptrauth-calls"
// DEFAULT-KERN-NOCALL-NOT: "-fptrauth-calls"
// DEFAULT-NOCALL: "-fptrauth-returns" "-fptrauth-intrinsics" "-fptrauth-indirect-gotos" "-fptrauth-auth-traps" "-fno-assume-unique-vtables" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-objc-isa-mode=sign-and-auth" "-target-cpu" "apple-a12"{{.*}}
// DEFAULT-KERN-NOCALL: "-fptrauth-returns" "-fptrauth-intrinsics" "-fptrauth-indirect-gotos" "-fptrauth-auth-traps" "-fno-assume-unique-vtables" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-objc-isa-mode=sign-and-auth" "-fptrauth-block-descriptor-pointers" "-fptrauth-vtable-pointer-address-discrimination" "-fptrauth-vtable-pointer-type-discrimination" "-fptrauth-function-pointer-type-discrimination" "-mbranch-target-enforce" "-target-cpu" "apple-a12"{{.*}}


// RUN: %clang -target arm64e-apple-ios15 -fno-ptrauth-returns -c %s -### 2>&1 | FileCheck %s --check-prefix NORET
// NORET-NOT: "-fptrauth-returns"

// RUN: %clang -target arm64e-apple-ios15 -fno-ptrauth-intrinsics -c %s -### 2>&1 | FileCheck %s --check-prefix NOINTRIN
// NOINTRIN-NOT: "-fptrauth-intrinsics"

// RUN: %clang -target arm64e-apple-ios15 -fno-ptrauth-auth-traps -c %s -### 2>&1 | FileCheck %s --check-prefix NOTRAP
// NOTRAP-NOT: "-fptrauth-auth-traps"
