import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftOptionalURL(TestBase):

    @requireNotEmbeddedSwift
    @skipIfLinux  # https://github.com/swiftlang/llvm-project/issues/13465
    @skipUnlessFoundationEssentials
    @swiftTest
    def test(self):
        """Test that po prints an optional URL"""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        self.expect('po u', substrs=['https://github.com'])
