"""
Test expression operations in class constrained protocols
"""

from __future__ import print_function


import os
import time
import re
import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *

class TestClassConstrainedProtocol(TestBase):
    @requireNotEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that we can reconstruct self and weak self captured in a
        closure, both in a method of a class conforming to a class constrained
        protocol and in a method of the protocol's extension."""
        self.build()
        src = lldb.SBFileSpec("main.swift")
        # main.swift reaches these in order.
        target, process, _, bkpt = lldbutil.run_to_source_breakpoint(
            self, "Break here in method", src
        )
        target.BreakpointDelete(bkpt.GetID())
        self.check_self("Break here in method", needs_dynamic=False)
        for bkpt_pattern in [
            "Break here for method weak self",
            "Break here in class protocol",
            "Break here for weak self",
        ]:
            threads = lldbutil.continue_to_source_breakpoint(
                self, process, bkpt_pattern, src
            )
            self.assertEqual(len(threads), 1, f"no stop at {bkpt_pattern}")
            self.check_self(bkpt_pattern, needs_dynamic=False)

    def setUp(self):
        # Call super's setUp().
        TestBase.setUp(self)

    def check_self(self, bkpt_pattern, needs_dynamic):
        opts = lldb.SBExpressionOptions()
        if needs_dynamic:
            opts.SetFetchDynamicValue(lldb.eNoDynamicValues)
            result = self.frame().EvaluateExpression("self", opts)
            error = result.GetError()
            self.assertTrue("self" in error.GetCString())
            self.assertTrue("run-target" in error.GetCString())
        opts.SetFetchDynamicValue(lldb.eDynamicCanRunTarget)
        result = self.frame().EvaluateExpression("self", opts)
        error = result.GetError()
        self.assertSuccess(error,
                           "'self' expression failed at '%s'" % bkpt_pattern)
        f_ivar = result.GetChildMemberWithName("f")
        self.assertTrue(f_ivar.IsValid(),
                        "Could not find 'f' in self at '%s'"%(bkpt_pattern))

        self.assertTrue(f_ivar.GetValueAsSigned() == 12345,
                        "Wrong value for f: %d"%(f_ivar.GetValueAsSigned()))
