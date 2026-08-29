import lldb

from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestSwiftDurationSummary(TestBase):
    @swiftTest
    def test_swift_duration_summary(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        self.expect("type category list", substrs=["swift"])
        self.expect(
            "type summary list -l swift",
            substrs=["Swift.Duration summary provider"],
        )

        values = [
            ("zero", "0.0 seconds"),
            ("simple", "3.033 seconds"),
            ("negative", "-90.0 seconds"),
            ("fractionOnly", "1.5e-05 seconds"),
            ("negativeFraction", "-1.5 seconds"),
            ("threshold", "0.001 seconds"),
            ("belowThreshold", "9.99e-04 seconds"),
        ]

        for name, summary in values:
            self.expect(f"frame var -d run -- {name}", substrs=[summary])
            self.expect(f"expr -d run -- {name}", substrs=[summary])
