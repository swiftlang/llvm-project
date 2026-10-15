"""
Test String.Index summary strings.

The test cases are LLDB versions of the original Swift tests, see
https://github.com/apple/swift/pull/58479
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    @requireNotEmbeddedSwift
    @skipUnlessFoundationEssentials
    @skipIfLinux  # https://github.com/swiftlang/llvm-project/issues/13465
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test(self):
        """Test String.Index summary strings for native and bridged Strings."""
        self.build()
        _, process, _, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        # The first breakpoint stop is a native (non-bridged) String.
        self.check_native()

        # The second breakpoint stop is a bridged String, which needs the
        # Objective-C Foundation.
        if self.platformIsDarwin():
            threads = lldbutil.continue_to_breakpoint(process, bkpt)
            self.assertEqual(len(threads), 1)
            self.check_bridged()

    def check_native(self):
        self.expect(
            "v nativeIndices",
            substrs=[
                "0[any]",
                "1[utf8]",
                "9[utf8]",
                "10[utf8]",
            ],
        )

        self.expect(
            "v unicodeScalarIndices",
            substrs=[
                "0[any]",
                "1[utf8]",
                "5[utf8]",
                "9[utf8]",
                "10[utf8]",
            ],
        )

        self.expect(
            "v utf8Indices",
            substrs=[
                "0[any]",
                "1[utf8]",
                "2[utf8]",
                "3[utf8]",
                "4[utf8]",
                "5[utf8]",
                "6[utf8]",
                "7[utf8]",
                "8[utf8]",
                "9[utf8]",
                "10[utf8]",
            ],
        )

        self.expect(
            "v utf16Indices",
            substrs=[
                "0[any]",
                "1[utf8]",
                "1[utf8]+1",
                "5[utf8]",
                "5[utf8]+1",
                "9[utf8]",
                "10[utf8]",
            ],
        )

    def check_bridged(self):
        self.expect(
            "v nativeIndices",
            substrs=[
                "0[any]",
                "1[utf16]",
                "5[utf16]",
                "6[utf16]",
            ],
        )

        self.expect(
            "v unicodeScalarIndices",
            substrs=[
                "0[any]",
                "1[utf16]",
                "3[utf16]",
                "5[utf16]",
                "6[utf16]",
            ],
        )

        self.expect(
            "v utf8Indices",
            substrs=[
                "0[any]",
                "1[utf16]",
                "1[utf16]+1",
                "1[utf16]+2",
                "1[utf16]+3",
                "3[utf16]",
                "3[utf16]+1",
                "3[utf16]+2",
                "3[utf16]+3",
                "5[utf16]",
                "6[utf16]",
            ],
        )

        self.expect(
            "v utf16Indices",
            substrs=[
                "0[any]",
                "1[utf16]",
                "2[utf16]",
                "3[utf16]",
                "4[utf16]",
                "5[utf16]",
                "6[utf16]",
            ],
        )
