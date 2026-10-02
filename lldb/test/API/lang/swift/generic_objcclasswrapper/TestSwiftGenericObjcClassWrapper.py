import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftGenericObjcClassWrapper(TestBase):

    def expr(self, frame, expression):
        options = lldb.SBExpressionOptions()
        options.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        value = frame.EvaluateExpression(expression, options)
        self.assertSuccess(value.GetError())
        return value

    def check_error(self, foo):
        lldbutil.check_variable(self, foo, typename="Foundation.NSError",
                                summary='domain: "patatino" - code: 0')
        lldbutil.check_variable(self, foo.GetChildMemberWithName("_userInfo"),
                                use_dynamic=True, summary="0 key/value pairs")

    @requireNotEmbeddedSwift
    @swiftTest
    @requireObjCFoundation
    def test(self):
        """Test that an ObjC object passed through a generic function is resolved"""
        self.build()
        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break 1", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        lldbutil.check_variable(self, self.expr(frame, "foo"),
                                typename="Foundation.NSError")

        threads = lldbutil.continue_to_source_breakpoint(
            self, process, "break 2", lldb.SBFileSpec("main.swift"))
        self.assertEqual(len(threads), 1)
        frame = threads[0].GetFrameAtIndex(0)
        self.check_error(frame.GetValueForVariablePath("foo"))
        self.check_error(self.expr(frame, "foo"))
