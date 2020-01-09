//===--- CmpResult.h - Enumeration of comparison results --------*- C++ -*-===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_AST_INTERP_CMPRESULT_H
#define LLVM_CLANG_AST_INTERP_CMPRESULT_H

#include "clang/AST/ComparisonCategories.h"

namespace clang {
namespace interp {

enum class CmpResult {
  Unequal,
  Less,
  Equal,
  Greater,
  Unordered,
};

inline ComparisonCategoryResult ToCCR(CmpResult Result) {
  switch (Result) {
  case CmpResult::Unequal:
    llvm_unreachable("should never produce Unequal for three-way comparison");
  case CmpResult::Less:
    return ComparisonCategoryResult::Less;
  case CmpResult::Equal:
    return ComparisonCategoryResult::Equal;
  case CmpResult::Greater:
    return ComparisonCategoryResult::Greater;
  case CmpResult::Unordered:
    return ComparisonCategoryResult::Unordered;
  }
  llvm_unreachable("invalid result");
}

} // namespace interp
}  // namespace clang

#endif
