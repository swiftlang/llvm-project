"""
Test that the synthetic children of a thick function (closure) can be printed.
"""

import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *


class TestSwiftThickFunctionChildren(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @swiftTest
    def test(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetSelectedFrame()

        self.expect(
            "frame variable -T --ptr-depth 1 fn",
            substrs=[
                "((Int) -> Int) fn = 0x",
                "(@convention(thin) () -> ()) function = 0x",
                "(Builtin.NativeObject) context = 0x",
            ],
        )

        fn = frame.FindVariable("fn")
        self.assertTrue(fn.IsValid())
        function = fn.GetChildMemberWithName("function")
        self.assertTrue(function.IsValid())
        self.assertEqual(function.GetTypeName(), "@convention(thin) () -> ()")
        self.assertTrue(function.GetValue().startswith("0x"))
