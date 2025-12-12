# TestSwiftDynamicValue.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2014 - 2016 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------
"""
Tests that dynamic values work correctly for Swift
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class SwiftDynamicValueTest(TestBase):
    @swiftTest
    def test_dynamic_value(self):
        """Tests that dynamic values work correctly for Swift"""
        self.build()
        self.dynamic_val_commands()

    def dynamic_val_commands(self):
        """Tests that dynamic values work correctly for Swift"""
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(self, "// Set a breakpoint here", lldb.SBFileSpec("main.swift"))

        self.expect(
            "frame variable -d no-dynamic",
            substrs=[
                "AWrapperClass) aWrapper",
                "SomeClass) anItem = ",
                "x = ",
                "Base<Int>) aBase = 0x",
                "v = 449493530"])
        self.expect(
            "frame variable --show-types",
            substrs=[
                "AWrapperClass) aWrapper",
                "YetAnotherClass) anItem = ",
                "x = ",
                "y = ",
                "z = ",
                "Derived<Int>) aBase = 0x",
                "Base<Int>)",
                ".Base<Swift.Int> = {",
                "v = 449493530",
                "q = 3735928559"])

        # Also test expression variables:
        frame = thread.frames[0]

        # Try a result variable:
        varobj = frame.EvaluateExpression("MakeASomeClass(.YetAnotherClass)")
        type = varobj.GetType()
        self.assertIn("YetAnotherClass", type.name, "Expression result dynamic type")
        z_var = varobj.GetChildMemberWithName("z")
        self.assertTrue(z_var.error.success, "Got the z ivar")
        z_val = z_var.GetValueAsSigned()
        self.assertEqual(z_val, 0xBEEF, "z had the right value")

        # Try a persistent expression variable as well:
        self.runCmd("expr var $test_var = MakeASomeClass(.SomeOtherClass)")
        persistent_var = frame.FindValue("$test_var", lldb.eValueTypeConstResult)
        self.assertTrue(persistent_var.error.success, "Got $test_var")
        type = persistent_var.GetType()
        self.assertIn("SomeOtherClass", type.name, "Got the right class")
        y_var = persistent_var.GetChildMemberWithName("y")
        self.assertTrue(y_var.error.success, "Got the y ivar")
        y_val = y_var.GetValueAsSigned()
        self.assertEqual(y_val, 0xFF, "Got y_val correctly")
        
        self.runCmd("continue")
        self.expect(
            "frame variable -d no-dynamic",
            substrs=[
                "AWrapperClass) aWrapper",
                "SomeClass) anItem = ",
                "x = ",
                "Base<Int>) aBase = 0x",
                "v = 449493530"])
        self.expect(
            "frame variable --show-types",
            substrs=[
                "AWrapperClass) aWrapper",
                "YetAnotherClass) anItem = ",
                "x = ",
                "y = ",
                "z = ",
                "Derived<Int>) aBase = 0x",
                "Base<Int>)",
                ".Base<Swift.Int> = {",
                "v = 449493530",
                "q = 3735928559"])
