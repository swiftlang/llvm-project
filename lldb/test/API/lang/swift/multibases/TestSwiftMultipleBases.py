# TestSwiftMultipleBases.py
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


class TestSwiftMultipleBases(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that a stored property inherited through several superclasses is shown"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        base = frame.FindVariable("base")
        lldbutil.check_variable(self, base.GetChildMemberWithName("name"),
                                summary='"hardcodedstring"')
        base = frame.FindVariable("derived1").GetChildAtIndex(0)
        lldbutil.check_variable(self, base.GetChildMemberWithName("name"),
                                summary='"hardcodedstring"')
        base = frame.FindVariable("derived2").GetChildAtIndex(0).GetChildAtIndex(0)
        lldbutil.check_variable(self, base.GetChildMemberWithName("name"),
                                summary='"hardcodedstring"')
