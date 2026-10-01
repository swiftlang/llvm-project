# TestSwiftPORecursiveBehavior.py
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


class TestSwiftPORecursiveBehavior(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that po of a cyclic object graph elides the repeated object"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect('po s', substrs=['▿ a : Optional', 'some', '0x', '{ ... }'])
