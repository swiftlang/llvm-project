import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestArchetypeInConditionalBreakpoint(TestBase):
    @skipEmbeddedSwift # rdar://184867874 (Embedded Swift: archetype names are unavailable in monomorphized frames, so expr T.self fails)
    @swiftTest
    def test_free_function(self):
        self.do_test("break here for free function")

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test_class(self):
        self.do_test("break here for class")

    def do_test(self, breakpoint_string):
        """Tests that using archetypes in a conditional breakpoint's expression works correctly"""
        self.build()
        target = lldbutil.run_to_breakpoint_make_target(self)

        breakpoint = target.BreakpointCreateBySourceRegex(
            breakpoint_string, lldb.SBFileSpec("main.swift")
        )

        # Make sure that we don't stop when the condition doesn't match. This
        # runs first so that the condition is evaluated in a fresh target.
        breakpoint.SetCondition("T.self == Double.self")
        launch_info = target.GetLaunchInfo()
        launch_info.SetWorkingDirectory(self.get_process_working_directory())
        error = lldb.SBError()
        process = target.Launch(launch_info, error)
        self.assertSuccess(error)
        self.assertEqual(process.state, lldb.eStateExited)

        breakpoint.SetCondition("T.self == Int.self")
        _, process, _, _ = lldbutil.run_to_breakpoint_do_run(self, target, breakpoint)
        self.assertEqual(process.state, lldb.eStateStopped)
        self.expect("expression T.self", substrs=["Int"])
