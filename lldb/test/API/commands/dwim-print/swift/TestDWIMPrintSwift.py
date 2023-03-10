"""
In Swift, test `po 0x12345600`, via dwim-print.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    def test_swift_po_address(self):
        self.build()
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "// break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.frame[0]
        addr = frame.FindVariable("object").GetLoadAddress()
        hex_addr = f"0x{addr:x}"
        self.expect(f"dwim-print -O -- {hex_addr}", substrs=[f"<Object: {hex_addr}>"])

    def test_swift_po_non_address_hex(self):
        """No special handling of non-memory hex values."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "// break here", lldb.SBFileSpec("main.swift")
        )
        self.expect(f"dwim-print -O -- 0x1000", substrs=["4096"])
