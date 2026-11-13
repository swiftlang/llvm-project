import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


def describe(value):
    value.SetPreferSyntheticValue(True)
    return (
        value.GetType().GetName(),
        value.GetValue(),
        value.GetSummary(),
        [describe(value.GetChildAtIndex(i)) for i in range(value.GetNumChildren())],
    )


class TestCase(lldbtest.TestBase):

    @swiftTest
    @skipEmbeddedSwift
    @skipIf(archs=["x86_64"], bugnumber="rdar://192464251")
    @skipIf(oslist=["linux"], archs=["aarch64"])
    def test(self):
        """Test the type and the return value of async funclets"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        for function, type_name in [
            ("f_int", "(Swift.Int) async -> Swift.Int"),
            ("f_string", "() async -> Swift.String"),
            ("f_two_int", "() async -> a.TwoInt"),
            ("f_throws", "() async throws -> Swift.Int"),
        ]:
            frame = thread.frames[0]
            self.assertIn(
                "resume partial function for a." + function, frame.GetFunctionName()
            )
            self.assertEqual(frame.GetFunction().GetType().GetName(), type_name)
            call_line = thread.frames[1].GetLineEntry().GetLine()
            thread.StepOut()
            return_value = thread.GetStopReturnValue()
            self.assertTrue(return_value.IsValid(), function)
            returned = describe(return_value)
            if thread.frames[0].GetLineEntry().GetLine() == call_line:
                thread.StepOver()
            variable = thread.frames[0].FindVariable(
                "v_" + function.removeprefix("f_")
            )
            self.assertEqual(returned, describe(variable))
