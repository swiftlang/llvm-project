# TestSwiftObjCInterop.py
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


class TestSwiftObjCInterop(TestBase):

    @swiftTest
    @requireSwiftObjCInterop
    def test(self):
        """Test that variables and expressions work in a program importing Dispatch"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("label"), summary='"lldbtest"')
        label = frame.EvaluateExpression("label")
        self.assertSuccess(label.GetError())
        lldbutil.check_variable(self, label, summary='"lldbtest"')
