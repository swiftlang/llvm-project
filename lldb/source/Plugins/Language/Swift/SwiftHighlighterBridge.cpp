//===----------------------------------------------------------------------===//
//
// This source file is part of the Swift open source project
//
// Copyright (c) 2024 Apple Inc. and the Swift project authors.
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
//
//===----------------------------------------------------------------------===//

#include "SwiftHighlighterBridge.h"
#include "swiftHighlighter/swiftHighlighter-swift.h"
#include <string>

using namespace lldb_private;
using namespace swiftHighlighter;

void SwiftHighlighterBridge::Highlight(const HighlightStyle &options,
                                 llvm::StringRef line,
                                 std::optional<size_t> cursor_pos,
                                 llvm::StringRef previous_lines,
                                 Stream &s) const {
  std::string highlighted = highlight(previous_lines.str(), line.str());
  s << highlighted;
}
