import platfrom
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestCase(TestBase):

    @swiftTest
    def test_summary_contains_name(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break outside", lldb.SBFileSpec("main.swift")
        )
        self.expect("v task", patterns=[r'"Chore" id:[1-9]'])

    @swiftTest
    def test_thread_contains_name(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break inside", lldb.SBFileSpec("main.swift")
        )

        expected_name = (
            r"DispatchWorker"
            if platform.system() == "Linux"
            else r"Chore \(Task [1-9]\)"
        )
        self.assertRegex(thread.name, expected_name)
