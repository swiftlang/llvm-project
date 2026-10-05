# TestSwiftGenericSelf.py
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
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftZeroSizeGenericSelf(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a zero-size generic self resolves to its bound type"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("self"), use_dynamic=use_dynamic,
                                typename="a.GenericSelf<Swift.String>")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        self_var = frame.FindVariable("self")
        lldbutil.check_variable(self, self_var, use_dynamic=use_dynamic,
                                typename="a.MyStruct<Swift.Int>")
        self.assertEqual(
            self_var.GetDynamicValue(lldb.eDynamicCanRunTarget).GetNumChildren(), 0)
