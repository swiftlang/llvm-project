"""
Test that Float16 reports a scalar value.
"""

import os

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


def isAmazonLinux2023():
    for path in ("/etc/os-release", "/usr/lib/os-release"):
        if os.path.exists(path):
            with open(path) as f:
                contents = f.read()
            if 'PLATFORM_ID="platform:al2023"' in contents:
                return "Amazon Linux 2023 is not supported."

    return None


class TestSwiftFloat16(TestBase):
    @swiftTest
    @skipEmbeddedSwiftOnWindows
    # Float16 is unavailable in the stdlib on x86_64 macOS.
    @skipIf(oslist=["macosx"], archs=["x86_64"])
    # The distro check inspects the host, which says nothing about the target
    # when the test runs against a remote platform.
    @skipIfRemote
    # Amazon Linux 2023 has no Float16 support.
    @skipTestIfFn(isAmazonLinux2023)
    def test(self):
        """Test that Float16 has a value, like Float and Double do."""
        self.build()

        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

        frame = thread.frames[0]
        self.assertTrue(frame, "Frame 0 is valid.")

        # The controls: both report a value rather than only a _value child.
        lldbutil.check_variable(self, frame.FindVariable("f32"), False, value="2.5")
        lldbutil.check_variable(self, frame.FindVariable("f64"), False, value="3.5")

        lldbutil.check_variable(self, frame.FindVariable("f16"), False, value="1.5")
