import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericExtensionsTypealias(TestBase):

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test(self):
        """Test that arguments of generic extension methods resolve to bound types"""
        self.build()
        # Embedded Swift specializes generic code, so these values are concrete
        # there and have no dynamic type to resolve.
        use_dynamic = not self.isEmbeddedSwift()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("rhs"), use_dynamic=use_dynamic,
                                typename="Swift.Array<Swift.Int>", summary="1 value")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        lldbutil.check_variable(self, frame.FindVariable("separator"), use_dynamic=use_dynamic,
                                typename="Swift.String")
