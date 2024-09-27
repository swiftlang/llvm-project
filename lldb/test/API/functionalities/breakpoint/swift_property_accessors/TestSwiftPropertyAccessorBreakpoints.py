import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestCase(TestBase):
    @swiftTest
    def test(self):
        """Test that a breakpoint on a property accessor can be set by name."""
        self.build()
        exe = self.getBuildArtifact("a.out")
        target = self.dbg.CreateTarget(exe)

        for name, alt_name in (
            ("read_only.get", None),
            ("read_write.get", None),
            ("read_write.set", None),
            ("observed.willSet", "observed.willset"),
            ("observed.didSet", "observed.didset"),
        ):
            bp = target.BreakpointCreateByName(name, "a.out")
            if not bp.IsValid() and alt_name:
                bp = target.BreakpointCreateByName(alt_name, "a.out")
            self.assertTrue(bp.IsValid(), f"{name} breakpoint failed")
            self.assertEqual(bp.num_locations, 1)

        # Setting a breakpoint on the name "get" should not create a breakpoint
        # matching property getters. The other accerssor suffixes should also
        # not succeed as bare names.
        for name in ("get", "set", "willSet", "didSet", "willset", "didset"):
            bp = target.BreakpointCreateByName(name, "a.out")
            self.assertEqual(bp.num_locations, 0, f"{name} breakpoint unexpected")
