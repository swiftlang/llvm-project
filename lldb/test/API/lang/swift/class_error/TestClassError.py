# TestSwiftGenericSelf.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2018 Apple Inc. and the Swift project authors
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


class TestClassError(TestBase):

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that generic Error-conforming class arguments resolve dynamically"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        pat = frame.FindVariable("Pat")
        lldbutil.check_variable(self, pat, use_dynamic=True, typename="a.MyErr")
        pat = pat.GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, pat.GetChildMemberWithName("x"), value="23")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        pat = frame.FindVariable("Pat")
        lldbutil.check_variable(self, pat, use_dynamic=True, typename="a.MyOtherErr")
        pat = pat.GetDynamicValue(lldb.eDynamicCanRunTarget)
        base = pat.GetChildAtIndex(0)
        lldbutil.check_variable(self, base.GetChildMemberWithName("x"), value="42")
