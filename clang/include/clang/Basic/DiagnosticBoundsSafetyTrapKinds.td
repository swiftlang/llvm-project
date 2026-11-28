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

// The trap reasons involving `__terminated_by` types describe the type and its
// terminator using these arguments (see
// `CodeGenFunction::AddTerminatedByTypeToTrapReason()`):
//
// %1 - The pointee type of the `__terminated_by` type.
// %2 - 0 if the terminator is zero, otherwise 1.
// %3 - The terminator value.
defvar TerminatedByType =
  "'%1 *%select{__null_terminated|__terminated_by(%3)}2'";
defvar TerminatedByTerm = "%select{null terminator|terminator '%3'}2";

// Trap reasons for the checks performed by
// `__unsafe_terminated_by_from_indexable(T, PTR [, PTR_TO_TERM])`.
//
// They all start by describing the conversion using these arguments:
//
// %0 - The `__indexable` type being converted from.
// %1-%3 - The `__terminated_by` type being converted to.
defvar TerminatedByFromIndexableConversion =
  !strconcat("converting %0 to ", TerminatedByType, " where ");

// Checks on the pointer being converted.
//
// %4 - The pointer being converted.
// %5 - The check that failed. This is appended by
//      `CodeGenFunction::EmitBoundsSafetyBoundsCheck()` so it uses the
//      `BoundsSafetyPtrCheckKind` values.
def trap_bs_terminated_by_from_indexable_ptr : Trap<!strconcat(
  TerminatedByFromIndexableConversion,
  "pointer %4 %select{"
    "is above its upper bound|"
    "overflows the address space|"
    "is below its lower bound"
  "}5")>;

// Checks on the pointer to the terminator.
//
// %4 - The pointer to the terminator.
// %5 - The pointer being converted.
// %6 - The check that failed.
def trap_bs_terminated_by_from_indexable_term_ptr : Trap<!strconcat(
  TerminatedByFromIndexableConversion,
  "pointer %4 to the ", TerminatedByTerm, " "
  "%enum_select<BoundsSafetyTermPtrCheckKind>{"
    "%Lower{is below the lower bound of %5}|"
    "%Overflow{overflows the address space}|"
    "%Upper{is above the upper bound of %5}"
  "}6")>;

// Checks that the terminator is found.
//
// %4 - The pointer being converted (`NotFound`) or the pointer to the
//      terminator (`NotTerm`).
// %5 - The check that failed.
def trap_bs_terminated_by_from_indexable_term : Trap<!strconcat(
  TerminatedByFromIndexableConversion,
  "%enum_select<BoundsSafetyTermCheckKind>{"
    "%NotFound{there is no ", TerminatedByTerm,
      " within the bounds of %4}|"
    "%NotTerm{pointer %4 does not point to the ",
      TerminatedByTerm, "}"
  "}5")>;

// Check that a `__terminated_by` pointer isn't incremented past its
// terminator.
//
// %0 - The expression incrementing the pointer.
// %1-%3 - The `__terminated_by` type of the pointer.
def trap_bs_terminated_by_ptr_arith : Trap<!strconcat(
  "incrementing ", TerminatedByType, " pointer past the ", TerminatedByTerm,
  " in %0")>;

}
}
