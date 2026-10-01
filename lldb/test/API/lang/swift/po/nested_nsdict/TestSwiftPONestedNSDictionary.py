# TestSwiftPONestedNSDictionary.py
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


class TestSwiftPONestedNSDictionary(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @requireObjCFoundation
    def test(self):
        """Test po of a dictionary with nested NSArray and NSDictionary values"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect('po a', ordered=False,
                    substrs=['Key1', 'Value1', 'Key2', '1234', '5678', 'Object',
                             'WAHHH', '2467'])
