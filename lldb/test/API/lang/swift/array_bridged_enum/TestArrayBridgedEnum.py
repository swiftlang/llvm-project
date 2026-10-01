import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestArrayBridgedEnum(TestBase):

    @requireNotEmbeddedSwift
    @swiftTest
    @requireObjCFoundation
    def test(self):
        """Test that array elements of an imported NS_CLOSED_ENUM show their case"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        for i, case in enumerate(["zero", "one", "two", "three"]):
            element = frame.GetValueForVariablePath("array[%d]" % i)
            self.assertEqual(element.GetName(), "[%d]" % i)
            lldbutil.check_variable(self, element, summary="." + case)
