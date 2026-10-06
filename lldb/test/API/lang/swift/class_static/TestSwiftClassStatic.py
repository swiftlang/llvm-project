# TestSwiftClassStatic.py
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


class TestSwiftClassStatic(TestBase):

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test(self):
        """Test that a static class property can be inspected"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        v = frame.FindVariable("v")
        lldbutil.check_variable(self, v.GetChildMemberWithName("a"), value="1")
        lldbutil.check_variable(self, v.GetChildMemberWithName("b"), value="2")
        shared = frame.EvaluateExpression("WithStatic.Shared")
        self.assertSuccess(shared.GetError())
        lldbutil.check_variable(self, shared.GetChildMemberWithName("a"), value="1")
        lldbutil.check_variable(self, shared.GetChildMemberWithName("b"), value="2")
