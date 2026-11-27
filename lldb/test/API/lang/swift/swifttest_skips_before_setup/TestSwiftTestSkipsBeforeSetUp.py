"""
Test that @swiftTest skips a test before setUp() runs when LLDB has no Swift
support, so that setUp() can rely on the Swift toolchain.
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftTestSkipsBeforeSetUp(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        TestBase.setUp(self)
        config = lldb.SBDebugger.GetBuildConfiguration()
        has_swift = config.GetValueForKey("swift").GetValueForKey("value")
        self.assertTrue(
            has_swift.GetBooleanValue(False),
            "setUp() ran although LLDB has no Swift support",
        )

    @swiftTest
    def test(self):
        pass
