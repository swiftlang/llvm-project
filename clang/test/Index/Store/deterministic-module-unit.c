// Verify that implicitly building and indexing a Clang module produces
// byte-for-byte identical module unit files and deterministic macro/record
// ordering regardless of ASLR or prior IdentifierInfo allocations in PP.macros().

// RUN: rm -rf %t
// RUN: split-file %s %t
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/mcp \
// RUN:   -I %t/include -index-store-path %t/store1 %t/tu.c
// RUN: rm -rf %t/mcp
// RUN: %clang_cc1 -fmodules -fimplicit-module-maps -fmodules-cache-path=%t/mcp \
// RUN:   -include %t/perturb.h -I %t/include -index-store-path %t/store2 %t/tu.c
// RUN: cmp %t/store1/v5/units/DetMod-*.pcm-* %t/store2/v5/units/DetMod-*.pcm-*
// RUN: c-index-test core -print-unit %t/store1 | FileCheck %s --check-prefix=UNIT
// RUN: c-index-test core -print-record %t/store1 | FileCheck %s --check-prefix=RECORD

// UNIT: module-name: DetMod
// UNIT: DEPEND START
// UNIT-DAG: Record | system | DetMod | {{.*}}alpha.h
// UNIT-DAG: Record | system | DetMod | {{.*}}beta.h
// UNIT: DEPEND END

// RECORD-DAG: macro/C | DET_ALPHA_01 | {{.*}}DET_ALPHA_01
// RECORD-DAG: macro/C | DET_ALPHA_20 | {{.*}}DET_ALPHA_20
// RECORD-DAG: macro/C | DET_BETA_01 | {{.*}}DET_BETA_01
// RECORD-DAG: macro/C | DET_BETA_20 | {{.*}}DET_BETA_20

//--- perturb.h
// Allocate extra identifiers before loading the module so IdentifierInfo* heap
// addresses in the second invocation hash into different DenseMap buckets.
#define PERTURB_ID_01 1
#define PERTURB_ID_02 2
#define PERTURB_ID_03 3
#define PERTURB_ID_04 4
#define PERTURB_ID_05 5
#define PERTURB_ID_06 6
#define PERTURB_ID_07 7
#define PERTURB_ID_08 8
#define PERTURB_ID_09 9
#define PERTURB_ID_10 10
#define PERTURB_ID_11 11
#define PERTURB_ID_12 12
#define PERTURB_ID_13 13
#define PERTURB_ID_14 14
#define PERTURB_ID_15 15
#define PERTURB_ID_16 16

//--- include/module.modulemap
module DetMod [system] {
  header "guard_only.h"
  header "alpha.h"
  header "beta.h"
  export *
}

//--- include/guard_only.h
#ifndef DET_MOD_GUARD_ONLY_H
#define DET_MOD_GUARD_ONLY_H
#endif

//--- include/alpha.h
#ifndef DET_MOD_ALPHA_H
#define DET_MOD_ALPHA_H
#define DET_ALPHA_01 1
#define DET_ALPHA_02 2
#define DET_ALPHA_03 3
#define DET_ALPHA_04 4
#define DET_ALPHA_05 5
#define DET_ALPHA_06 6
#define DET_ALPHA_07 7
#define DET_ALPHA_08 8
#define DET_ALPHA_09 9
#define DET_ALPHA_10 10
#define DET_ALPHA_11 11
#define DET_ALPHA_12 12
#define DET_ALPHA_13 13
#define DET_ALPHA_14 14
#define DET_ALPHA_15 15
#define DET_ALPHA_16 16
#define DET_ALPHA_17 17
#define DET_ALPHA_18 18
#define DET_ALPHA_19 19
#define DET_ALPHA_20 20
void det_alpha_fn(void);
#endif

//--- include/beta.h
#ifndef DET_MOD_BETA_H
#define DET_MOD_BETA_H
#define DET_BETA_01 1
#define DET_BETA_02 2
#define DET_BETA_03 3
#define DET_BETA_04 4
#define DET_BETA_05 5
#define DET_BETA_06 6
#define DET_BETA_07 7
#define DET_BETA_08 8
#define DET_BETA_09 9
#define DET_BETA_10 10
#define DET_BETA_11 11
#define DET_BETA_12 12
#define DET_BETA_13 13
#define DET_BETA_14 14
#define DET_BETA_15 15
#define DET_BETA_16 16
#define DET_BETA_17 17
#define DET_BETA_18 18
#define DET_BETA_19 19
#define DET_BETA_20 20
void det_beta_fn(void);
#endif

//--- tu.c
#include "alpha.h"
void use_det_mod(void) { det_alpha_fn(); }
