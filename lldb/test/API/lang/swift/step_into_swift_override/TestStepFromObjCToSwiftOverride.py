import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil

class TestStepFromObjCToSwiftOverride(TestBase):
    """Test that you can step from an ObjC method of a class into the swift
       override of one of the methods."""

    @requireObjCInterop
    @swiftTest
    @requireNotEmbeddedSwift
    def test(self):
        self.build()
        (target, process, thread, breakpoint) = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("Foo.m")
        )

        lldbutil.ignore_swift_stdlib_when_stepping(platform, self)
        # Step in from the ObjC method to the swift implementation:
        thread.StepInto()
        self.assertEqual(thread.stop_reason, lldb.eStopReasonPlanComplete)
        self.assertIn("SwiftFoo.doSomething", thread.frames[0].GetFunctionName())
