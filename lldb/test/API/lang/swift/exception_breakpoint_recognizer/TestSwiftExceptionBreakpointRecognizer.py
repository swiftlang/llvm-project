import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
from lldbsuite.test import lldbutil


class TestCase(TestBase):

    @swiftTest
    def test(self):
        self.build()

        target = lldbutil.run_to_breakpoint_make_target(self)
        bp = target.BreakpointCreateForException(lldb.eLanguageTypeSwift, False, True)

        # First breakpoint in an untyped throws function.
        _, process, _, _ = lldbutil.run_to_breakpoint_do_run(self, target, bp)
        thread = process.selected_thread
        stop_desc = thread.GetStopDescription(128)
        self.assertEqual(stop_desc, "Swift exception breakpoint")
        self.assertEqual(thread.frame[0].symbol.name, "swift_willThrow")
        self.assertEqual(thread.selected_frame.idx, 1)

        # Second breakpoint in an typed throws function.
        process.Continue()
        thread = process.selected_thread
        stop_desc = thread.GetStopDescription(128)
        self.assertEqual(stop_desc, "Swift exception breakpoint")
        self.assertEqual(thread.frame[0].symbol.name, "swift_willThrowTypedImpl")
        self.assertEqual(thread.selected_frame.idx, 2)
