// Check that each warning is reported under the expected warning group, and
// that the user and SDK variants are enabled independently.
//
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=NONE --allow-empty --implicit-check-not=warning:
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=USER --implicit-check-not=warning:
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-forges %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=USER --implicit-check-not=warning:
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-sdk %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SDK --implicit-check-not=warning:
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-forges-sdk %s 2>&1 \
// RUN:   | FileCheck %s --check-prefix=SDK --implicit-check-not=warning:
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe -Wbounds-safety-unsafe-sdk %s 2>&1 \
// RUN:   | FileCheck %s --check-prefixes=USER,SDK --implicit-check-not=warning:

// NONE-NOT: warning:
// The SDK header is included first, so its warning is emitted first.
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_single' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_terminated_by' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_null_terminated' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_bidi_indexable' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_single' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_terminated_by' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_null_terminated' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// SDK: unsafe-forges-sdk-api.h:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_bidi_indexable' is unsafe [-Wbounds-safety-unsafe-forges-sdk]
// USER: unsafe-forges-warning-group.c:{{[0-9]+}}:{{[0-9]+}}: warning: use of '__unsafe_forge_single' is unsafe [-Wbounds-safety-unsafe-forges]

#include <ptrcheck.h>
#include "unsafe-forges-sdk-api.h"

int *user_forge(int *p) {
  return __unsafe_forge_single(int *, p);
}
