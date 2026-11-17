// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -verify=none %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-sdk -verify=sdk %s
// RUN: %clang_cc1 -fsyntax-only -fbounds-safety -Wbounds-safety-unsafe-forges-sdk -verify=sdk %s

// Tests -Wbounds-safety-unsafe-sdk(-forges-sdk) for the forges inside the SDK
// header unsafe-forges-sdk-api.h, which holds the expected diagnostics. See
// unsafe-forges-api.c for the non-SDK variants.

// none-no-diagnostics
#include <ptrcheck.h>
#include "unsafe-forges-sdk-api.h"
