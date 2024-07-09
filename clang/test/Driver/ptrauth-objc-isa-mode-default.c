// Test objc isa signing default based on target.

// AUTH: "-fptrauth-objc-isa-mode=sign-and-auth"
// STRIP: "-fptrauth-objc-isa-mode=sign-and-strip"

// Enabled on iOS14.5+, tvOS14.5+.
// RUN: %clang -target arm64e-apple-ios14.5            -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-tvos14.5           -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH

// Hence, enabled on iOS15+, tvOS15+.
// RUN: %clang -target arm64e-apple-ios15              -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-tvos15             -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH

// Disabled on older OS versions
// RUN: %clang -target arm64e-apple-ios14.4.0          -c %s -### 2>&1 | FileCheck %s --check-prefix STRIP
// RUN: %clang -target arm64e-apple-tvos14.4.0         -c %s -### 2>&1 | FileCheck %s --check-prefix STRIP

// Enabled on any macOS, watchOS, MacABI.
// RUN: %clang -target arm64e-apple-macos12            -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-macos11            -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-ios15-macabi       -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-ios14-macabi       -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-watchos5           -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH

// Enabled on any simulator targets as well.
// RUN: %clang -target arm64e-apple-ios14-simulator    -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-tvos14-simulator   -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -target arm64e-apple-watchos7-simulator -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH

// Kernel doesn't care (but is exposed to block isa)
// RUN: %clang -mkernel -target arm64e-apple-ios15     -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
// RUN: %clang -fapple-kext -target arm64e-apple-ios15 -c %s -### 2>&1 | FileCheck %s --check-prefix AUTH
