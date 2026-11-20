//==--- DiagnosticBoundsSafetyTrapKinds.td --------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
// These are the trap diagnostics used by -fbounds-safety
//===----------------------------------------------------------------------===//

let Component = "Trap" in {
let CategoryName = "Bounds check failed" in {

// Used for falling back to the legacy infrastructure for constructing
// trap reason strings.
def trap_bs_fallback : Trap<"%0">;

// TODO: Split these up once we know what the diagnostics strings needs to be.
// Ideally we want to be able to generate trap codes and opt-remark strings
// from these definitions too so we'll need to split them up when we do that.
def trap_bs_upper_lower_overflow_bound : Trap<
  "%enum_select<BoundsCheckContextKind>{"
    "%Indexing{indexing}|"
    "%TODO{TODO}"
  "}0 %enum_select<BoundsSafetyPtrCheckKind>{"
    "%Upper{above upper bound}|"
    "%Overflow{overflows address space}|"
    "%Lower{below lower bound"
  "}}2 in %1">;

// Checks on the pointer to the terminator passed to
// `__unsafe_terminated_by_from_indexable(T, PTR, PTR_TO_TERM)`.
//
// %0 - The `__indexable` type being converted from.
// %1 - The pointee type of the `__terminated_by` type being converted to.
// %2 - 0 if the terminator is zero, otherwise 1.
// %3 - The terminator value.
// %4 - The pointer to the terminator.
// %5 - The pointer being converted.
// %6 - The check that failed.
def trap_bs_terminated_by_from_indexable_term_ptr : Trap<
  "converting %0 to "
  "'%1 *%select{__null_terminated|__terminated_by(%3)}2' where pointer %4 to "
  "the %select{null terminator|terminator '%3'}2 "
  "%enum_select<BoundsSafetyTermPtrCheckKind>{"
    "%Lower{is below the lower bound of %5}|"
    "%Overflow{overflows the address space}|"
    "%Upper{is above the upper bound of %5}"
  "}6">;

}
}
