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
    @skipIf(oslist=["macosx"], bugnumber="rdar://26051759")
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a struct declared inside a closure can be inspected"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        s = frame.FindVariable("s")
        lldbutil.check_variable(self, s.GetChildMemberWithName("i"), value="777")
        s = frame.EvaluateExpression("s")
        self.assertSuccess(s.GetError())
        lldbutil.check_variable(self, s.GetChildMemberWithName("i"), value="777")
