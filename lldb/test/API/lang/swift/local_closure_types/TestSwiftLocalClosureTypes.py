# TestSwiftLocalClosureTypes.py
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


class TestSwiftLocalClosureTypes(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnLinux
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a struct declared inside a closure can be inspected"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        s = frame.FindVariable("s")
        lldbutil.check_variable(self, s.GetChildMemberWithName("i"), value="777")
        s = frame.EvaluateExpression("s")
        self.assertSuccess(s.GetError())
        lldbutil.check_variable(self, s.GetChildMemberWithName("i"), value="777")

        frame = self.continue_to(process, "break in first closure")
        s = frame.FindVariable("s")
        lldbutil.check_variable(self, s.GetChildMemberWithName("i"), value="1")

        frame = self.continue_to(process, "break in second closure")
        s = frame.FindVariable("s")
        lldbutil.check_variable(self, s.GetChildMemberWithName("a"), value="2")
        lldbutil.check_variable(self, s.GetChildMemberWithName("b"), value="3")

        frame = self.continue_to(process, "break in nested closure")
        t = frame.FindVariable("t")
        lldbutil.check_variable(self, t.GetChildMemberWithName("k"), value="4")

    def continue_to(self, process, marker):
        threads = lldbutil.continue_to_source_breakpoint(
            self, process, marker, lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        return threads[0].GetFrameAtIndex(0)
