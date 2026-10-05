# TestSwiftGenericSelf.py
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


class TestSwiftAssociatedSelfType(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a value of an associated type of Self resolves to its bound type"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        x = frame.FindVariable("x").GetDynamicValue(lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, x.GetChildMemberWithName("key"), value="2")
        lldbutil.check_variable(self, x.GetChildMemberWithName("value"), value="2")
