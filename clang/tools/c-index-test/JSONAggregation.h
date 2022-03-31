//===--- JSONAggregation.h - Index data aggregation in JSON format --------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#ifndef LLVM_CLANG_TOOLS_CINDEXTEST_JSONAGGREGATION_H
#define LLVM_CLANG_TOOLS_CINDEXTEST_JSONAGGREGATION_H

#include "clang/Basic/LLVM.h"
#include <map>

namespace clang {
namespace index {

/// Returns true if an error occurred, false otherwise.
bool aggregateDataAsJSON(StringRef StorePath,
						 std::map<std::string, std::string, std::greater<std::string>> PrefixMap,
						 raw_ostream &OS);

} // end namespace index
} // end namespace clang

#endif
