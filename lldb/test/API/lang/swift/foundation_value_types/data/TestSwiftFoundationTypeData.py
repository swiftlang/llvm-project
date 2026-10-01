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


class TestSwiftFoundationTypeData(TestBase):

    def expr(self, frame, expression):
        value = frame.EvaluateExpression(expression)
        self.assertSuccess(value.GetError())
        return value

    def check_data(self, frame, summary):
        lldbutil.check_variable(self, frame.FindVariable("data"), summary=summary)
        lldbutil.check_variable(self, self.expr(frame, "data"), summary=summary)

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIf(oslist=["linux"])
    @skipIf(
        bugnumber="rdar://60396797",  # should work but crashes.
        setting=("symbols.use-swift-clangimporter", "false"),
    )
    def test(self):
        """Test the Data summary for the empty, inline and slice representations"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        self.check_data(thread.GetFrameAtIndex(0), "0 bytes")

        for marker, summary in [("break 2", "3 bytes"), ("break 3", "259 bytes")]:
            threads = lldbutil.continue_to_source_breakpoint(
                self, process, marker, lldb.SBFileSpec("main.swift"))
            self.assertEqual(len(threads), 1)
            frame = threads[0].GetFrameAtIndex(0)
            self.check_data(frame, summary)
            first_byte = self.expr(
                frame,
                "data.subdata(in: data.startIndex ..< data.index(after: data.startIndex))")
            lldbutil.check_variable(self, first_byte, summary="1 byte")
