# TestSwiftFoundationValueTypes.py
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


class TestSwiftFoundationTypeDate(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIf(oslist=["linux"])
    def test(self):
        """Test the summary of a Date variable and expression result"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        summary = "1970-01-01 23:00:00 UTC"
        lldbutil.check_variable(self, frame.FindVariable("date"), summary=summary)
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        date = frame.EvaluateExpression("date", options)
        self.assertSuccess(date.GetError())
        lldbutil.check_variable(self, date, summary=summary)
