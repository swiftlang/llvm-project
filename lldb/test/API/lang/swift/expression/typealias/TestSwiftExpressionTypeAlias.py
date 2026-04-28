import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftExpressionTypeAlias(lldbtest.TestBase):

    @skipEmbeddedSwift
    @swiftTest
    def test(self):
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        self.expect('expr -d run -- local', substrs=['Pair<Int>'])
        process.Continue()
        # FIXME!
        self.expect('expr -d run -- local', substrs=['(Int, Int)'])
        process.Continue()
        self.expect("frame var associated", substrs=['Self as a.MyProtocolBase.MyAlias'])
        self.expect("expr associated", substrs=['a.B as a.MyProtocolBase.MyAlias'])
