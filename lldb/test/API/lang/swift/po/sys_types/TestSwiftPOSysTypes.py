# TestSwiftPOSysTypes.py
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


class TestSwiftPOSysTypes(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @requireSwiftObjCInterop
    def test(self):
        """Test po on Swift standard library and Foundation types"""
        self.build()
        _, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        # Make sure po doesn't escape non-printables.
        self.expect("po num", substrs=["\\n", "\""], matching=False)
        self.expect("po num", substrs=['22'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po str", substrs=['Hello world'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 3", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po arr", substrs=['1', '2', '3', '4'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 4", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po nsarr", substrs=['1', '2', '3', '4'])
        # may change depending on OS/platform
        self.expect("po clr", substrs=['1 0 0 1'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 5", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        # may change depending on OS/platform
        self.expect("po nsobject", substrs=['<NSObject: 0x'])
        self.expect(
            "script lldb.frame.FindVariable('nsobject').GetObjectDescription()",
            substrs=['<NSObject: 0x'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 6", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po any", substrs=['1234'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 7", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po notification", substrs=['JustANotification'])
        self.expect("po notification", matching=False, substrs=['super'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 8", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect("po lines", startstr='one\ndue')
