# TestSwiftArraySliceFormatters.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------
"""
Test ArraySlice synthetic types.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    @swiftTest
    def test_swift_array_slice_formatters(self):
        """Test ArraySlice synthetic types."""
        self.build()
        _, _, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        # TODO: Fix `arraySubSequence` rdar://92898538
        for var in ("someSlice", "arraySlice"):
            self.expect(f"v {var}", substrs=[f"{var} = 2 values", "[1] = 2", "[2] = 3"])
