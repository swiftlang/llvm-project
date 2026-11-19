//===-- SwiftStdlibFormatters.h ---------------------------------*- C++ -*-===//
//
// This source file is part of the Swift.org open source project
//
// Copyright (c) 2026 Apple Inc. and the Swift project authors
// Licensed under Apache License v2.0 with Runtime Library Exception
//
// See https://swift.org/LICENSE.txt for license information
// See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
//
//===----------------------------------------------------------------------===//

#ifndef liblldb_SwiftStdlibFormatters_h_
#define liblldb_SwiftStdlibFormatters_h_

#include "lldb/DataFormatters/TypeSynthetic.h"

namespace lldb_private::formatters::swift {
SyntheticChildrenFrontEnd *
UniqueBoxSyntheticFrontEndCreator(CXXSyntheticChildren *, lldb::ValueObjectSP);
}

#endif
