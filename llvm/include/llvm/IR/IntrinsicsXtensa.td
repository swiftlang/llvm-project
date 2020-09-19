//===- IntrinsicsXtensa.td - Defines Xtensa intrinsics -----*- tablegen -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file defines all of the Xtensa-specific intrinsics.
//
//===----------------------------------------------------------------------===//

let TargetPrefix = "xtensa" in {  // All intrinsics start with "llvm.xtensa.".

def int_xtensa_umul_aa_ll: GCCBuiltin<"__builtin_xtensa_umul_aa_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_umul_aa_hl: GCCBuiltin<"__builtin_xtensa_umul_aa_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_umul_aa_lh: GCCBuiltin<"__builtin_xtensa_umul_aa_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_umul_aa_hh: GCCBuiltin<"__builtin_xtensa_umul_aa_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;

def int_xtensa_mul_aa_ll: GCCBuiltin<"__builtin_xtensa_mul_aa_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mul_aa_hl: GCCBuiltin<"__builtin_xtensa_mul_aa_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mul_aa_lh: GCCBuiltin<"__builtin_xtensa_mul_aa_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mul_aa_hh: GCCBuiltin<"__builtin_xtensa_mul_aa_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;

def int_xtensa_mul_ad_ll: GCCBuiltin<"__builtin_xtensa_mul_ad_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mul_ad_hl: GCCBuiltin<"__builtin_xtensa_mul_ad_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mul_ad_lh: GCCBuiltin<"__builtin_xtensa_mul_ad_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mul_ad_hh: GCCBuiltin<"__builtin_xtensa_mul_ad_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;

def int_xtensa_mul_da_ll: GCCBuiltin<"__builtin_xtensa_mul_da_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mul_da_hl: GCCBuiltin<"__builtin_xtensa_mul_da_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mul_da_lh: GCCBuiltin<"__builtin_xtensa_mul_da_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mul_da_hh: GCCBuiltin<"__builtin_xtensa_mul_da_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;

def int_xtensa_mul_dd_ll: GCCBuiltin<"__builtin_xtensa_mul_dd_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mul_dd_hl: GCCBuiltin<"__builtin_xtensa_mul_dd_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mul_dd_lh: GCCBuiltin<"__builtin_xtensa_mul_dd_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mul_dd_hh: GCCBuiltin<"__builtin_xtensa_mul_dd_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;

def int_xtensa_mula_aa_ll: GCCBuiltin<"__builtin_xtensa_mula_aa_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mula_aa_hl: GCCBuiltin<"__builtin_xtensa_mula_aa_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mula_aa_lh: GCCBuiltin<"__builtin_xtensa_mula_aa_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_mula_aa_hh: GCCBuiltin<"__builtin_xtensa_mula_aa_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;

def int_xtensa_mula_ad_ll: GCCBuiltin<"__builtin_xtensa_mula_ad_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mula_ad_hl: GCCBuiltin<"__builtin_xtensa_mula_ad_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mula_ad_lh: GCCBuiltin<"__builtin_xtensa_mula_ad_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_mula_ad_hh: GCCBuiltin<"__builtin_xtensa_mula_ad_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;

def int_xtensa_mula_da_ll: GCCBuiltin<"__builtin_xtensa_mula_da_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mula_da_hl: GCCBuiltin<"__builtin_xtensa_mula_da_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mula_da_lh: GCCBuiltin<"__builtin_xtensa_mula_da_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_mula_da_hh: GCCBuiltin<"__builtin_xtensa_mula_da_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;

def int_xtensa_mula_dd_ll: GCCBuiltin<"__builtin_xtensa_mula_dd_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mula_dd_hl: GCCBuiltin<"__builtin_xtensa_mula_dd_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mula_dd_lh: GCCBuiltin<"__builtin_xtensa_mula_dd_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_mula_dd_hh: GCCBuiltin<"__builtin_xtensa_mula_dd_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;

def int_xtensa_muls_aa_ll: GCCBuiltin<"__builtin_xtensa_muls_aa_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_muls_aa_hl: GCCBuiltin<"__builtin_xtensa_muls_aa_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_muls_aa_lh: GCCBuiltin<"__builtin_xtensa_muls_aa_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;
def int_xtensa_muls_aa_hh: GCCBuiltin<"__builtin_xtensa_muls_aa_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], []>;

def int_xtensa_muls_ad_ll: GCCBuiltin<"__builtin_xtensa_muls_ad_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_muls_ad_hl: GCCBuiltin<"__builtin_xtensa_muls_ad_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_muls_ad_lh: GCCBuiltin<"__builtin_xtensa_muls_ad_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;
def int_xtensa_muls_ad_hh: GCCBuiltin<"__builtin_xtensa_muls_ad_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<1>]>;

def int_xtensa_muls_da_ll: GCCBuiltin<"__builtin_xtensa_muls_da_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_muls_da_hl: GCCBuiltin<"__builtin_xtensa_muls_da_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_muls_da_lh: GCCBuiltin<"__builtin_xtensa_muls_da_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;
def int_xtensa_muls_da_hh: GCCBuiltin<"__builtin_xtensa_muls_da_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>]>;

def int_xtensa_muls_dd_ll: GCCBuiltin<"__builtin_xtensa_muls_dd_ll">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_muls_dd_hl: GCCBuiltin<"__builtin_xtensa_muls_dd_hl">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_muls_dd_lh: GCCBuiltin<"__builtin_xtensa_muls_dd_lh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;
def int_xtensa_muls_dd_hh: GCCBuiltin<"__builtin_xtensa_muls_dd_hh">,
  Intrinsic<[], [llvm_i32_ty, llvm_i32_ty], [ImmArg<0>, ImmArg<1>]>;


def int_xtensa_mula_da_ll_lddec: GCCBuiltin<"__builtin_xtensa_mula_da_ll_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_lh_lddec: GCCBuiltin<"__builtin_xtensa_mula_da_lh_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_hl_lddec: GCCBuiltin<"__builtin_xtensa_mula_da_hl_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_hh_lddec: GCCBuiltin<"__builtin_xtensa_mula_da_hh_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;

def int_xtensa_mula_da_ll_ldinc: GCCBuiltin<"__builtin_xtensa_mula_da_ll_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_lh_ldinc: GCCBuiltin<"__builtin_xtensa_mula_da_lh_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_hl_ldinc: GCCBuiltin<"__builtin_xtensa_mula_da_hl_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;
def int_xtensa_mula_da_hh_ldinc: GCCBuiltin<"__builtin_xtensa_mula_da_hh_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>]>;

def int_xtensa_mula_dd_ll_lddec: GCCBuiltin<"__builtin_xtensa_mula_dd_ll_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_lh_lddec: GCCBuiltin<"__builtin_xtensa_mula_dd_lh_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_hl_lddec: GCCBuiltin<"__builtin_xtensa_mula_dd_hl_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_hh_lddec: GCCBuiltin<"__builtin_xtensa_mula_dd_hh_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;

def int_xtensa_mula_dd_ll_ldinc: GCCBuiltin<"__builtin_xtensa_mula_dd_ll_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_lh_ldinc: GCCBuiltin<"__builtin_xtensa_mula_dd_lh_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_hl_ldinc: GCCBuiltin<"__builtin_xtensa_mula_dd_hl_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;
def int_xtensa_mula_dd_hh_ldinc: GCCBuiltin<"__builtin_xtensa_mula_dd_hh_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty, llvm_i32_ty, llvm_i32_ty],
            [ImmArg<0>, ImmArg<2>, ImmArg<3>]>;

//===----------------------------------------------------------------------===//
// Load operations

def int_xtensa_lddec: GCCBuiltin<"__builtin_xtensa_lddec">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty], [ImmArg<0>]>;

def int_xtensa_ldinc: GCCBuiltin<"__builtin_xtensa_ldinc">,
  Intrinsic<[], [llvm_i32_ty, llvm_ptr_ty], [ImmArg<0>]>;

//===----------------------------------------------------------------------===//
// WSR/XSR/RSR

def int_xtensa_wsr_acclo: GCCBuiltin<"__builtin_xtensa_wsr_acclo">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_acclo: GCCBuiltin<"__builtin_xtensa_rsr_acclo">,
  Intrinsic<[llvm_i32_ty], [], []>;

def int_xtensa_xsr_acclo: GCCBuiltin<"__builtin_xtensa_xsr_acclo">,
  Intrinsic<[], [llvm_ptr_ty], []>;

def int_xtensa_wsr_acchi: GCCBuiltin<"__builtin_xtensa_wsr_acchi">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_acchi: GCCBuiltin<"__builtin_xtensa_rsr_acchi">,
  Intrinsic<[llvm_i32_ty], [], []>;

def int_xtensa_xsr_acchi: GCCBuiltin<"__builtin_xtensa_xsr_acchi">,
  Intrinsic<[], [llvm_ptr_ty], []>;

def int_xtensa_wsr_m0: GCCBuiltin<"__builtin_xtensa_wsr_m0">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_m0: GCCBuiltin<"__builtin_xtensa_rsr_m0">,
  Intrinsic<[llvm_i32_ty]>;

def int_xtensa_xsr_m0: GCCBuiltin<"__builtin_xtensa_xsr_m0">,
  Intrinsic<[], [llvm_ptr_ty], []>;

def int_xtensa_wsr_m1: GCCBuiltin<"__builtin_xtensa_wsr_m1">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_m1: GCCBuiltin<"__builtin_xtensa_rsr_m1">,
  Intrinsic<[llvm_i32_ty], [], []>;

def int_xtensa_xsr_m1: GCCBuiltin<"__builtin_xtensa_xsr_m1">,
  Intrinsic<[], [llvm_ptr_ty], []>;

def int_xtensa_wsr_m2: GCCBuiltin<"__builtin_xtensa_wsr_m2">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_m2: GCCBuiltin<"__builtin_xtensa_rsr_m2">,
  Intrinsic<[llvm_i32_ty], [], []>;

def int_xtensa_xsr_m2: GCCBuiltin<"__builtin_xtensa_xsr_m2">,
  Intrinsic<[], [llvm_ptr_ty], []>;

def int_xtensa_wsr_m3: GCCBuiltin<"__builtin_xtensa_wsr_m3">,
  Intrinsic<[], [llvm_i32_ty], []>;

def int_xtensa_rsr_m3: GCCBuiltin<"__builtin_xtensa_rsr_m3">,
  Intrinsic<[llvm_i32_ty], [], []>;

def int_xtensa_xsr_m3: GCCBuiltin<"__builtin_xtensa_xsr_m3">,
  Intrinsic<[], [llvm_ptr_ty], []>;

}
