# TestSwiftPrivateGenericSelf.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2019 Apple Inc. and the Swift project authors
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


class TestSwiftPrivateGenericSelf(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a generic self bound to a private type resolves dynamically"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        lldbutil.check_variable(self, self.frame().FindVariable("self"),
                                use_dynamic=True, use_synthetic=True,
                                typename='a.MyStruct<a.Outer.CodingKeys>')
