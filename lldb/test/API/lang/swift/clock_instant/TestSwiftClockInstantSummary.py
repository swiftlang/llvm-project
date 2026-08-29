import lldb

from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class TestSwiftClockInstantSummary(TestBase):
    @skipEmbeddedSwift
    @swiftTest
    def test_clock_instant_summary(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        self.expect("type category list", substrs=["swift"])
        self.expect(
            "type summary list -l swift",
            substrs=[
                "Swift ContinuousClock.Instant summary provider",
                "Swift SuspendingClock.Instant summary provider",
            ],
        )

        values = [
            "continuous",
            "suspending",
        ]

        # Clock.now is not deterministic, so check the shape of the summary:
        # e.g. 1055919.15305175 seconds
        summary_pattern = r"[0-9]+\.[0-9]+ seconds"

        for name in values:
            self.expect(
                f"frame var -d run -- {name}",
                patterns=[summary_pattern],
            )
            self.expect(
                f"expr -d run -- {name}",
                patterns=[summary_pattern],
            )
