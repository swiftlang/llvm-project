# TestSwiftProtocolOptional.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2018 Apple Inc. and the Swift project authors
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


class TestSwiftProtocolOptional(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a value of associated type bound to an Optional resolves"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("patatino"),
                                use_dynamic=True,
                                typename="Swift.Optional<Swift.Int>", value="5")
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        patatino = frame.EvaluateExpression("patatino", options)
        self.assertSuccess(patatino.GetError())
        lldbutil.check_variable(self, patatino,
                                typename="Swift.Optional<Swift.Int>", value="5")
