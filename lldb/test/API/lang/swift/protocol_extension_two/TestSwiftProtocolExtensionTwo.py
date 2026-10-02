import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftProtocolExtensionTwo(TestBase):

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        """Test evaluating self and a computed property in a constrained extension"""
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"))
        frame = thread.GetFrameAtIndex(0)
        options = lldb.SBExpressionOptions()
        baciotto = frame.EvaluateExpression("self.baciotto", options)
        self.assertSuccess(baciotto.GetError())
        lldbutil.check_variable(self, baciotto, typename="Swift.Int", value="0")
        pat = frame.EvaluateExpression("self", options)
        self.assertSuccess(pat.GetError())
        lldbutil.check_variable(self, pat, typename="a.Patatino<a.Winky>")
