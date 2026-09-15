import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEntryValue(TestBase):
    @skipIfWindows  # this needs DWARF
    @swiftTest
    @skipIf(archs=no_match(["arm64", "arm64e", "x86_64"]))
    def test_entry_value_in_parent_frame(self):
        """Test that a clobbered variable only recoverable from the parent frame still prints correctly"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        # frame #0 is callee(); its caller still has `a` and `b` in scope.
        frame = thread.GetFrameAtIndex(1)
        self.assertIn("caller", frame.GetFunctionName())

        # Precondition: this only exercises the fix if the location really is
        # an entry value, so a codegen change cannot make it pass for the wrong
        # reason.
        self.expect(
            "image lookup -v -a %d" % frame.GetPC(),
            substrs=["DW_OP_entry_value"],
            msg="expected an entry-value location in caller()",
        )

        self.assertEqual(frame.FindVariable("a").GetValue(), "11")
        self.assertEqual(frame.FindVariable("b").GetValue(), "22")
