import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericStructWithOptional(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @skipIfLinux
    def test(self):
        """Test a generic struct holding an Optional<Data> and an enum in a closure"""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        self.expect("expr -O -- e",
                    substrs=["StructWithGenericContents<Optional<Data>, CustomError>"])

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        e = frame.FindVariable("e", lldb.eDynamicCanRunTarget)
        lldbutil.check_variable(self, e.GetChildMemberWithName("field1"),
                                summary="0 bytes")
        lldbutil.check_variable(self, e.GetChildMemberWithName("field2"), value="err1")
