import re

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

    def return_value_problems(self, functions):
        """Step out of each function in functions and return how the values
        they returned or threw differ from v_<name>, the variables run()
        assigns them to."""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "// step out", lldb.SBFileSpec("main.swift")
        )
        problems = []
        seen = []
        while thread:
            function = re.search(
                r"(\w+)(<[^>]*>)?\(", thread.frames[0].GetFunctionName()
            )[1]
            if function in functions:
                seen.append(function)
                call_line = thread.frames[1].GetLineEntry().GetLine()
                thread.StepOut()
                return_value = thread.GetStopReturnValue()
                if not return_value.IsValid():
                    return_value = thread.GetStopErrorValue()
                returned = describe(return_value) if return_value.IsValid() else None
                if thread.frames[0].GetLineEntry().GetLine() == call_line:
                    thread.StepOver()
                variable = thread.frames[0].FindVariable(
                    "v_" + function.removeprefix("f_")
                )
                expected = describe(variable)
                if returned != expected:
                    problems.append("%s: %s != %s" % (function, returned, expected))
            process.Continue()
            thread = lldbutil.get_stopped_thread(process, lldb.eStopReasonBreakpoint)
        self.assertEqual(sorted(seen), sorted(functions))
        return problems

    @swiftTest
    @skipEmbeddedSwift
    @skipIf(archs=["x86_64"], bugnumber="rdar://192464251")
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
            ("f_typed_throws", "() async throws(a.E) -> Swift.Int"),
            ("f_typed_throws_large", "() async throws(a.LargeE) -> Swift.Int"),
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

    @swiftTest
    @skipEmbeddedSwift
    @skipIf(archs=["x86_64"], bugnumber="rdar://192464251")
    def test_thrown_error(self):
        """Test the error thrown by an async function"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break in throwing", lldb.SBFileSpec("main.swift")
        )
        thread.StepOut()
        self.assertFalse(thread.GetStopReturnValue().IsValid())
        self.assertEqual(thread.GetStopErrorValue().GetTypeName(), "a.E")

    @swiftTest
    @skipEmbeddedSwift
    @skipIf(archs=["x86_64"], bugnumber="rdar://192464251")
    def test_values(self):
        """Test the values returned by async functions"""
        functions = [
            "f_two_double",
            "f_two_float",
            "f_wrapped_double",
            "f_nested_floats",
            "f_double_tuple",
            "f_optional_int",
            "f_optional_nil",
            "f_optional_class",
            "f_optional_string",
            "f_array",
            "f_dictionary",
        ]
        self.assertEqual(self.return_value_problems(functions), [])

    @swiftTest
    @skipEmbeddedSwift
    @skip
    def test_known_problems(self):
        """Test the values lldb gets wrong in async functions"""
        functions = [
            # FIXME: Swift returns the float in a floating-point register.
            "f_float_int",
            "f_float_int_tuple",
            # FIXME: Swift returns up to four registers, not two.
            "f_three_int",
            "f_payload",
            # FIXME: Swift returns this in memory, at an address lldb doesn't
            # know after the return.
            "f_existential",
            # FIXME: This function has the type () -> ().
            "f_generic",
            # FIXME: For a typed error, the error register holds 1, not the
            # error.
            "f_typed_throwing",
            "f_typed_throwing_large",
        ]
        self.assertEqual(self.return_value_problems(functions), [])
