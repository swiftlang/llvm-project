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


class TestSwiftFoundationTypeIndexPath(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIf(oslist=['windows', 'linux'])
    def test(self):
        """Test the IndexPath summary for each storage representation"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("path"),
                                summary="5 indices")
        lldbutil.check_variable(self, frame.FindVariable("short_path"),
                                summary="2 indices")
        lldbutil.check_variable(self, frame.FindVariable("very_short_path"),
                                summary="1 index")
        lldbutil.check_variable(self, frame.FindVariable("empty_path"),
                                summary="0 indices")
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        path = frame.EvaluateExpression("path", options)
        self.assertSuccess(path.GetError())
        lldbutil.check_variable(self, path, summary="5 indices")
