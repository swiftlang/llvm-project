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

#ifndef liblldb_SwiftHighlighterBridge_h_
#define liblldb_SwiftHighlighterBridge_h_

#include "lldb/Core/Highlighter.h"
#include "lldb/Utility/Stream.h"

namespace lldb_private {

/// A "bridge" that calls into the Swift code that does Syntax highlighting.
/// This class's purpose is to mplement the Highlighter interface, as Swift/C++
/// interop doesn't allow for a Swift class to inherit from a C++ one, and
/// forward the calls of "Highlight" to Swift.
class SwiftHighlighterBridge : public Highlighter {

public:
  SwiftHighlighterBridge() = default;

  llvm::StringRef GetName() const override { return "swift"; }

  void Highlight(const HighlightStyle &options, llvm::StringRef line,
                 std::optional<size_t> cursor_pos,
                 llvm::StringRef previous_lines, Stream &s) const override;
};
} // namespace lldb_private

#endif // liblldb_SwiftHighlighterBridge_h_
