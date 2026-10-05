# TestSwiftPORefTypes.py
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


class TestSwiftPORefTypes(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test po of classes with default and custom mirrors and descriptions"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect("po dm", substrs=['y12', 'q24'], matching=False)
        self.expect("po dm", substrs=['DefaultMirror: 0x'])
        self.expect("po cm", substrs=['t36'])
        self.expect("po cm", substrs=['y12', 'q24'], matching=False)
        self.expect("po cs", substrs=['CustomDebugStringConvertible'])
        self.expect("po cs", substrs=['CustomStringConvertible'], matching=False)
        self.expect("po cs", substrs=['y12', 'q24'], matching=False)
        self.expect("po (dm as Any,cm as Any, 48 as Any)", substrs=['t36', '48'])
        self.expect("po (dm as Any,cm as Any, 48 as Any)", substrs=['y12', 'q24'],
                    matching=False)
        self.expect("po td", substrs=['TheDescendant', 'y12', 'q24'])
        self.expect("po td", substrs=['t36'], matching=False)
        self.expect("po tr",
                    substrs=['TheReflectiveDescendant', 'super', 'y12', 'q24', 'w48'])
        self.expect("po tr", substrs=['t36'], matching=False)
        self.expect("script lldb.frame.FindVariable('tr').GetObjectDescription()",
                    substrs=['t36'], matching=False)
        self.expect("script lldb.frame.FindVariable('tr').GetObjectDescription()",
                    substrs=['TheReflectiveDescendant', 'y12', 'q24', 'w48'])
