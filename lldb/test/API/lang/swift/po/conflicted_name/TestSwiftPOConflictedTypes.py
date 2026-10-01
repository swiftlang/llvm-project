# TestSwiftPOConflictedTypes.py
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


class TestSwiftPOConflictedTypes(TestBase):

    # rdar://185128962 (Embedded Swift: po falls back to p, so object-description output is unavailable)
    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test po of a class whose name shadows a standard library type"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect("po m", substrs=['Fun with mirrors'])
