# TestSwiftNonmodularInclude.py
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


class TestSwiftNonmodularInclude(TestBase):

    @swiftTest
    @skipIf(oslist=['windows'])
    def test(self):
        """Test expressions on a Clang type from a module with a nonmodular include"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        foo = thread.GetFrameAtIndex(0).EvaluateExpression("foo")
        self.assertSuccess(foo.GetError(), "import worked")
        lldbutil.check_variable(self, foo.GetChildMemberWithName("i"), value="42")
