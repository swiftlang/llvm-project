# TestSwiftFoundationTypeUUID.py
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


class TestSwiftFoundationTypeUUID(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIf(oslist=["linux"])
    def test(self):
        """Test the summary of a Foundation.UUID value"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        uuid_string = "AE5DE240-397B-4D09-9B99-D38E4CBC9952"
        lldbutil.check_variable(self, frame.FindVariable("uuid"), summary=uuid_string)

        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        uuid = frame.EvaluateExpression("uuid", options)
        self.assertSuccess(uuid.GetError())
        lldbutil.check_variable(self, uuid, summary=uuid_string)
