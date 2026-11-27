"""
The fields of every class in a hierarchy are part of a class instance's layout,
including those of a superclass that is a specialization of a generic class.
Embedded Swift emits no reflection metadata, so the layout comes from DWARF.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEmbeddedGenericSuperclassChain(TestBase):

    @swiftTest
    @skipUnlessEmbeddedSwift
    def test(self):
        self.build()
        self.runCmd("setting set symbols.swift-enable-ast-context false")
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.expect("frame variable leaf", substrs=["a = 1", "x = 7", "b = 2"])
