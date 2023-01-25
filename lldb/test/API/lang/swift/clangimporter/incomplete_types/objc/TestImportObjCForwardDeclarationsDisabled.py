import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil
import unittest2

class ImportObjCForwardDeclarationsDisabledInLLDBExpressionEvaluationTest(TestBase):

    @skipIf(setting=('symbols.use-swift-clangimporter', 'false'))
    @swiftTest
    def test(self):
        """Tests that values whose type was imported by the ClangImporter as a placeholder for a forward declared type cannot be used in LLDB expression evaluation"""
        self.build()
        lldbutil.run_to_source_breakpoint(self,"break here", lldb.SBFileSpec("main.swift"))
        self.expect("p foo", substrs=["aasdf"])

