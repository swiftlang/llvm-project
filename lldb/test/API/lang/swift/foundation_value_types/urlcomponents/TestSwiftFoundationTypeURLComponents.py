# TestSwiftFoundationTypeURLComponents.py
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


class TestSwiftFoundationTypeURLComponents(TestBase):

    @swiftTest
    @requireObjCFoundation
    @expectedFailureAll(bugnumber='rdar://32800121')
    def test(self):
        """Test that the fields of a Foundation.URLComponents value are displayed"""
        self.build()
        _, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        # These stay commands: lldb has never displayed these fields (see the
        # expectedFailure), so there are no known exact values to assert on.
        self.expect('frame variable -d run -- urlc',
                    substrs=['urlString = "https://www.apple.com:12345/thisurl/isnotreal/itoldyou.php?page=fake"'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect('frame variable -d run --  urlc', substrs=['scheme = "https"'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 3", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect('frame variable -d run --  urlc', substrs=['host = "www.apple.com"'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 4", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect('frame variable -d run --  urlc',
                    substrs=['port = 0x', 'Int64(12345)'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 5", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect('frame variable -d run --  urlc',
                    substrs=['path = "/thisurl/isnotreal/itoldyou.php"'])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 6", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        self.expect('frame variable -d run --  urlc', substrs=['query = "page=fake"'])
        self.expect('expression -d run --  urlc',
                    substrs=['urlString = "https://www.apple.com:12345/thisurl/isnotreal/itoldyou.php?page=fake"',
                             'scheme = "https"', 'user = nil', 'fragment = nil'])
