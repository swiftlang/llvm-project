# TestSwiftReferenceCount.py
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


class TestSwiftReferenceCount(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test the language swift refcount command on class, struct and unknown names"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect('language swift refcount Blah', substrs=['cannot find \'Blah\''],
                    error=True)
        self.expect('language swift refcount LiveObj',
                    substrs=['(strong =', 'unowned =', 'weak ='])
        self.expect('language swift refcount MyStruct',
                    substrs=['refcount only available for class types'], error=True)
