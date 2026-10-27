"""
Test that we can print and call closures passed in various contexts
"""

import os
import re
import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


def check_not_captured_error(test, frame, var_name, parent_function):
    expected_error = (
        f"A variable named '{var_name}' existed in function '{parent_function}'"
    )
    value = frame.EvaluateExpression(var_name)
    error = value.GetError().GetCString()
    test.assertIn(expected_error, error)

    value = frame.EvaluateExpression(f"1 + {var_name} + 1")
    error = value.GetError().GetCString()
    test.assertIn(expected_error, error)

    test.expect(f"frame variable {var_name}", substrs=[expected_error], error=True)


def check_no_enhanced_diagnostic(test, frame, var_name):
    forbidden_str = "A variable named"
    value = frame.EvaluateExpression(var_name)
    error = value.GetError().GetCString()
    test.assertNotIn(forbidden_str, error)

    value = frame.EvaluateExpression(f"1 + {var_name} + 1")
    error = value.GetError().GetCString()
    test.assertNotIn(forbidden_str, error)

    test.expect(
        f"frame variable {var_name}",
        substrs=[forbidden_str],
        matching=False,
        error=True,
    )


class TestSwiftClosureVarNotCaptured(TestBase):
    def continue_to(self, process, bkpt_name):
        """Continue to bkpt_name and return the thread stopped there. Async
        code may resume on a different thread, so always use the returned
        thread rather than one from an earlier stop."""
        threads = lldbutil.continue_to_source_breakpoint(
            self, process, bkpt_name, lldb.SBFileSpec("main.swift")
        )
        self.assertEqual(len(threads), 1, f"expected one thread at {bkpt_name}")
        return threads[0]

    # main.swift exercises every scenario sequentially, so a single process
    # visits all of them in order. Each breakpoint is deleted after it is hit,
    # which matters for the closures passed to map() over multiple elements.
    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test(self):
        self.build()
        target, process, _, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break_simple_closure", lldb.SBFileSpec("main.swift")
        )
        target.BreakpointDelete(bkpt.GetID())
        # Async variable inspection on Linux/Windows are still problematic.
        test_async = self.getPlatform() not in ["linux", "windows"]

        self.check_simple_closure(process)
        self.check_nested_closure(process)
        if test_async:
            self.check_async_closure(process)
        self.check_ctor_class_closure(process)
        self.check_ctor_struct_closure(process)
        self.check_ctor_enum_closure(process)
        if test_async:
            self.check_task_inside_non_async_func(process)

    def check_simple_closure(self, process):
        frame = process.GetSelectedThread().frames[0]
        check_not_captured_error(self, frame, "var_in_foo", "func_1(arg:)")
        check_not_captured_error(self, frame, "arg", "func_1(arg:)")
        check_no_enhanced_diagnostic(self, frame, "dont_find_me")

    def check_nested_closure(self, process):
        frame = self.continue_to(process, "break_double_closure_1").frames[0]
        check_not_captured_error(self, frame, "var_in_foo", "func_2(arg:)")
        check_not_captured_error(self, frame, "arg", "func_2(arg:)")
        check_not_captured_error(
            self, frame, "var_in_outer_closure", "closure #1 in func_2(arg:)"
        )
        check_no_enhanced_diagnostic(self, frame, "dont_find_me")

        frame = self.continue_to(process, "break_double_closure_2").frames[0]
        check_not_captured_error(self, frame, "var_in_foo", "func_2(arg:)")
        check_not_captured_error(self, frame, "arg", "func_2(arg:)")
        check_not_captured_error(
            self, frame, "var_in_outer_closure", "closure #1 in func_2(arg:)"
        )
        check_not_captured_error(
            self, frame, "shadowed_var", "closure #1 in func_2(arg:)"
        )
        check_no_enhanced_diagnostic(self, frame, "dont_find_me")

    def check_async_closure(self, process):
        for bkpt_name in ["break_async_closure_1", "break_async_closure_2"]:
            frame = self.continue_to(process, bkpt_name).frames[0]
            check_not_captured_error(self, frame, "var_in_foo", "func_3(arg:)")
            check_not_captured_error(self, frame, "arg", "func_3(arg:)")
            check_not_captured_error(
                self, frame, "var_in_outer_closure", "closure #1 in func_3(arg:)"
            )
            check_no_enhanced_diagnostic(self, frame, "dont_find_me")

    def check_task_inside_non_async_func(self, process):
        frame = self.continue_to(
            process, "break_task_inside_non_async_function"
        ).frames[0]
        check_not_captured_error(self, frame, "x", "task_inside_non_async_function()")

    def check_ctor_and_static(self, process, kind, type_name):
        frame = self.continue_to(process, f"break_ctor_{kind}").frames[0]
        check_not_captured_error(self, frame, "input", f"{type_name}.init(input:)")
        check_not_captured_error(self, frame, "find_me", f"{type_name}.init(input:)")
        check_no_enhanced_diagnostic(self, frame, "dont_find_me")

        frame = self.continue_to(process, f"break_static_member_{kind}").frames[0]
        static_func = f"static {type_name}.static_func(input_static:)"
        check_not_captured_error(self, frame, "input_static", static_func)
        check_not_captured_error(self, frame, "find_me_static", static_func)
        check_no_enhanced_diagnostic(self, frame, "dont_find_me_static")

    def check_computed_properties(self, process, kind, type_name):
        for accessor in ["getter", "setter"]:
            frame = self.continue_to(
                process, f"break_{kind}_computed_property_{accessor}"
            ).frames[0]
            check_not_captured_error(
                self,
                frame,
                "find_me",
                f"{type_name}.{kind}_computed_property.{accessor}",
            )
            check_no_enhanced_diagnostic(self, frame, "dont_find_me")

        frame = self.continue_to(
            process, f"break_{kind}_computed_property_didset"
        ).frames[0]
        check_not_captured_error(
            self,
            frame,
            "find_me",
            f"{type_name}.{kind}_computed_property_didset.didset",
        )
        check_no_enhanced_diagnostic(self, frame, "dont_find_me")

    def check_ctor_class_closure(self, process):
        self.check_ctor_and_static(process, "class", "MY_CLASS")
        self.check_computed_properties(process, "class", "MY_CLASS")

    def check_ctor_struct_closure(self, process):
        self.check_ctor_and_static(process, "struct", "MY_STRUCT")
        self.check_computed_properties(process, "struct", "MY_STRUCT")

    def check_ctor_enum_closure(self, process):
        self.check_ctor_and_static(process, "enum", "MY_ENUM")
