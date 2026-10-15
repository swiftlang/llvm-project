# TestSwiftInstancePointerSetSP.py
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
"""
Test that we correctly track instance pointers in ValueObjectPrinter
"""
import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftInstancePointerSetSP(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)

    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test_instancepointerset_sp(self):
        """Test that we correctly track instance pointers in ValueObjectPrinter"""
        self.build()
        self.do_test()

    def setUp(self):
        lldbtest.TestBase.setUp(self)
        self.main_source = "main.swift"
        self.main_source_spec = lldb.SBFileSpec(self.main_source)

    def do_test(self):
        """Test that we correctly track instance pointers in ValueObjectPrinter"""
        lldbutil.run_to_source_breakpoint(
            self, 'break here', self.main_source_spec)

        self.expect(
            "frame variable -d run -- o",
            substrs=[
                '"Hello World"',
                '{...}'],
            matching=True)
        self.expect(
            "expression -d run -- o",
            substrs=[
                '"Hello World"',
                '{...}'],
            matching=True)

