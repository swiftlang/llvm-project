import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    """Test variable inspection in a variadic generic method of a variadic
    generic type, whose generic signature has a pack shape requirement."""

    NO_DEBUG_INFO_TESTCASE = True

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()

        target, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))

        self_var = self.frame().FindVariable("self")
        # FIXME: This doesn't produce any useful output, but also should not
        # crash. Once this actually produces a value, this test should be
        # updated.
        self.assertTrue(self_var.GetError())

