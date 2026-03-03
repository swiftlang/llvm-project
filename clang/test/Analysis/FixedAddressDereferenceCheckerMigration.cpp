// Testing all explicit combinations of the old and new checkers.

// -old => no-warning
// RUN: %clang_analyze_cc1 -analyzer-checker=core \
// RUN:   -analyzer-disable-checker=core.FixedAddressDereference \
// RUN:   -verify=disabled-old %s
// disabled-old-no-diagnostics

// +old => warn to use the new checker name
// RUN: %clang_analyze_cc1 -analyzer-checker=core %s 2>&1 \
// RUN:   -analyzer-checker=core.FixedAddressDereference \
// RUN: | FileCheck %s --check-prefix=WARN-USE-NEW-CHECKER-NAME
// WARN-USE-NEW-CHECKER-NAME:      warning: checker 'core.FixedAddressDereference' was renamed to 'optin.core.FixedAddressDereference'.
// WARN-USE-NEW-CHECKER-NAME-SAME: Enable the new checker name or stop explicitly enabling the old checker name

// -new => no-warning
// RUN: %clang_analyze_cc1 -analyzer-checker=core \
// RUN:   -analyzer-disable-checker=optin.core.FixedAddressDereference \
// RUN:   -verify=disabled-new %s
// disabled-new-no-diagnostics

// +new => no-warning
// RUN: %clang_analyze_cc1 -analyzer-checker=core \
// RUN:   -analyzer-checker=optin.core.FixedAddressDereference \
// RUN:   -verify=optin-core-checker %s
// disabled-new-no-diagnostics

// -old,-new => no-warning
// RUN: %clang_analyze_cc1 -analyzer-checker=core \
// RUN:   -analyzer-disable-checker=core.FixedAddressDereference \
// RUN:   -analyzer-disable-checker=optin.core.FixedAddressDereference \
// RUN:   -verify=disabled-old-disabled-new %s
// disabled-old-disabled-new-no-diagnostics

// +old,-new => warn to use the new checker name
// RUN: %clang_analyze_cc1 -analyzer-checker=core %s 2>&1 \
// RUN:   -analyzer-checker=core.FixedAddressDereference \
// RUN:   -analyzer-disable-checker=optin.core.FixedAddressDereference \
// RUN: | FileCheck %s --check-prefix=WARN-USE-NEW-CHECKER-NAME

// -old,+new => warn: finds the issue
// RUN: %clang_analyze_cc1 -analyzer-checker=core \
// RUN:   -analyzer-disable-checker=core.FixedAddressDereference \
// RUN:   -analyzer-checker=optin.core.FixedAddressDereference \
// RUN:   -verify=optin-core-checker %s

// +old,+new => warn: finds the issue, and also emits the warning about the old checker name
// RUN: %clang_analyze_cc1 -analyzer-checker=core %s 2>&1 \
// RUN:   -analyzer-checker=core.FixedAddressDereference \
// RUN:   -analyzer-checker=optin.core.FixedAddressDereference \
// RUN: | FileCheck %s --check-prefixes=WARN-USE-NEW-CHECKER-NAME,FINDING
// FINDING: dereference of a fixed address [optin.core.FixedAddressDereference]

// Testing the default configuration:
// By default, the 'optin.core.FixedAddressDereference' should be NOT enabled.
// RUN: %clang_analyze_cc1 -analyzer-checker=core -verify=default-optin-not-enabled %s
// default-optin-not-enabled-no-diagnostics

// Test that -Werror does not transform this warning into an error.
// -Werror, +old => warn to use the new checker name; not an ERROR!
// RUN: %clang_analyze_cc1 -analyzer-checker=core %s 2>&1 \
// RUN:   -Werror \
// RUN:   -analyzer-checker=core.FixedAddressDereference \
// RUN:   -analyzer-disable-checker=optin.core.FixedAddressDereference \
// RUN: | FileCheck %s --check-prefix=WARN-USE-NEW-CHECKER-NAME

void test() {
  int *p = (int *)0x020;
  int x = p[0]; // optin-core-checker-warning {{dereference of a fixed address [optin.core.FixedAddressDereference]}}
}
