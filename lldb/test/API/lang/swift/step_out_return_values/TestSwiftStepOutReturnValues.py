import re

import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


def describe(value, depth=3):
    value.SetPreferSyntheticValue(True)
    children = range(value.GetNumChildren()) if depth else []
    return (
        value.GetType().GetName(),
        value.GetValue(),
        value.GetSummary(),
        [describe(value.GetChildAtIndex(i), depth - 1) for i in children],
    )


class TestCase(lldbtest.TestBase):

    def return_value_problems(self, functions):
        """Step out of each function in functions and return how the values
        they returned or threw differ from v_<name>, the variables main()
        assigns them to."""
        self.build()
        target = self.createTestTarget()
        module = target.GetExecutable().GetFilename()
        for function in functions:
            # A getter has no base name.
            if not target.BreakpointCreateByName(function, module).GetNumLocations():
                target.BreakpointCreateByRegex(function + r"\.getter", module)
        process = target.LaunchSimple(None, None, self.get_process_working_directory())
        problems = []
        seen = []
        thread = lldbutil.get_stopped_thread(process, lldb.eStopReasonBreakpoint)
        while thread:
            function = re.search(
                r"(\w+)(<[^>]*>)?(\(|\.getter)", thread.frames[0].GetFunctionName()
            )[1]
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
    @skipIfAsan
    @skipIf(archs=["x86_64"], bugnumber="rdar://192462106")
    def test(self):
        """Test the values returned by Swift functions"""
        functions = [
            "f_int",
            "f_int8",
            "f_uint32",
            "f_bool",
            "f_float",
            "f_double",
            "f_class",
            "f_string",
            "f_long_string",
            "f_two_int8",
            "f_two_int",
            "f_bool_int32",
            "f_has_class",
            "f_closure",
            "f_throws",
            "f_typed_throws",
            "f_typed_throws_large",
            "f_no_payload",
            "f_raw_value",
            "f_tuple",
            "answer",
            "increment",
            "staticAnswer",
            "computed",
        ]
        if self.isEmbeddedSwift():
            functions += [
                "f_optional_int",
                "f_optional_nil",
                "f_optional_class",
                "f_optional_string",
                "f_array",
            ]
        else:
            # rdar://192368824
            functions.append("f_throwing")
        self.assertEqual(self.return_value_problems(functions), [])
        _, _, thread, _ = lldbutil.run_to_name_breakpoint(self, "f_int")
        thread.StepOut()
        self.expect("thread info", patterns=[r"Return value: \(Int\) \$R\d+ = 42\n"])

    @swiftTest
    @expectedFailureAll
    def test_known_problems(self):
        """Test the values lldb gets wrong"""
        functions = [
            # FIXME: TypeSystemSwift reports no homogeneous aggregates.
            "f_two_double",
            "f_two_float",
            "f_wrapped_double",
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
        ]
        if not self.isEmbeddedSwift():
            # FIXME: These have a value only in Embedded Swift. Printing the
            # dictionary there crashes the type system validation.
            functions += [
                "f_optional_int",
                "f_optional_nil",
                "f_optional_class",
                "f_optional_string",
                "f_array",
                "f_dictionary",
            ]
        else:
            # FIXME: In Embedded Swift, lldb shows no thrown error.
            # rdar://192368824
            functions.append("f_throwing")
        self.assertEqual(self.return_value_problems(functions), [])
