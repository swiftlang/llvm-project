"""
Test Substring summary strings.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    # @skipUnlessFoundation
    @swiftTest
    def test_swift_substring_formatters(self):
        """Test Subtring summary strings."""
        self.build()

        # First stop tests small strings.

        _, process, _, _, = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        self.expect(
            "v substrings",
            substrs=[
                '[0] = "abc"',
                '[1] = "bc"',
                '[2] = "c"',
                '[3] = ""',
            ],
        )

        # Continue to second stop, to test non-small strings.

        process.Continue()

        # Strings larger that 15 bytes (64-bit) or 10 bytes (32-bit) cannot be
        # stored directly inside the String struct.
        alphabet = "abcdefghijklmnopqrstuvwxyz"
        subalphabets = [alphabet[i:] for i in range(len(alphabet) + 1)]

        self.expect(
            "v substrings",
            substrs=[
                f'[{i}] = "{substring}"'
                for i, substring in zip(range(26), subalphabets)
            ],
        )
