import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):

    @skipIfWindows
    @swiftTest
    def test(self):
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self._do_test(thread, value=23)
        process.Continue()
        self._do_test(thread, value=41)

    def _do_test(self, thread: lldb.SBThread, value: int):
        frame = thread.selected_frame
        x = frame.FindVariable("x")
        self.assertEqual(x.GetNumChildren(), 1)
        child = x.GetChildMemberWithName("value")
        self.assertEqual(child.GetName(), "value")
        self.assertEqual(child.GetValueAsSigned(), value)
        self.assertEqual(child.GetID(), x.GetChildAtIndex(0).GetID())
