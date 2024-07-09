// Check the ABI version support defaults.

// RUN: %clang                                                                 -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang                                 -mkernel                        -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix KERNELABIVERSION
// RUN: %clang                                 -fapple-kext                    -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix KERNELABIVERSION
//
// RUN: %clang -fno-ptrauth-kernel-abi-version -mkernel                        -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang -mkernel                        -fno-ptrauth-kernel-abi-version -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang -fno-ptrauth-kernel-abi-version -fapple-kext                    -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang -fapple-kext                    -fno-ptrauth-kernel-abi-version -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang -fno-ptrauth-kernel-abi-version -fptrauth-kernel-abi-version    -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
// RUN: %clang -fptrauth-kernel-abi-version    -fno-ptrauth-kernel-abi-version -target arm64e-apple-ios -c %s -### 2>&1 | FileCheck %s --check-prefix ABIVERSION-DEFAULT --check-prefix NOKERNELABIVERSION
//
// ABIVERSION-DEFAULT: "-fptrauth-abi-version=0"
// KERNELABIVERSION: "-fptrauth-kernel-abi-version"
// NOKERNELABIVERSION-NOT: fptrauth-kernel-abi-version
