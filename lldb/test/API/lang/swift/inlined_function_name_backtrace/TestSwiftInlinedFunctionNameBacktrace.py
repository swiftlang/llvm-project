"""
Test that inlined Swift frames print their own name and source location in a
backtrace.
"""
import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftInlinedFunctionNameBacktrace(TestBase):
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test_inlined_function_names_in_backtrace(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_name_breakpoint(self, "baz")

        # A frame that is not reported as inlined would still match the
        # backtrace patterns below minus the "[inlined]" marker, so check the
        # structure of the stack first to get a diagnosable failure when the
        # optimizer stops inlining.
        for index, name in ((1, "foo"), (2, "bar")):
            frame = thread.GetFrameAtIndex(index)
            self.assertTrue(
                frame.IsInlined(),
                "expected frame #%d to be inlined, but it is '%s'; the "
                "optimizer may no longer be inlining %s"
                % (index, frame.GetFunctionName(), name),
            )

        baz_line = line_number("main.swift", "return a * 2")
        foo_line = line_number("main.swift", "return a * b * baz(a)")
        bar_line = line_number("main.swift", "let result = foo(4, 5)")
        self.expect(
            "thread backtrace",
            patterns=[
                r"frame #0: .*`baz\(a=.*\) at main\.swift:%d:\d+ \[opt\]" % baz_line,
                r"frame #1: .*`foo\(a=.*, b=.*\) at main\.swift:%d:\d+ \[opt\] \[inlined\]"
                % foo_line,
                r"frame #2: .*`bar\(\) at main\.swift:%d:\d+ \[opt\] \[inlined\]"
                % bar_line,
            ],
        )
