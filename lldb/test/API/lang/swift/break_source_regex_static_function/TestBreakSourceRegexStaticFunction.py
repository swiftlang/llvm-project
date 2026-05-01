"""
Tests source regex breakpoints scoped to Swift static functions (-X option).
"""

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):
    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test_source_regex_with_static_function_filter(self):
        self.build()
        target, _, _, _ = lldbutil.run_to_name_breakpoint(self, "main")
        self.expect("breakpoint set -p 'break here' -X first")
        bp = target.breakpoint[-1]
        self.assertTrue(bp.IsEnabled())
        self.assertEqual(bp.GetNumResolvedLocations(), 1)
        self.assertIn("Scope.first", str(bp.location[0]))

    @skipEmbeddedSwiftOnWindows
    @swiftTest
    def test_source_regex_with_static_function_filter_and_file(self):
        self.build()
        target, _, _, _ = lldbutil.run_to_name_breakpoint(self, "main")
        self.expect("breakpoint set -f main.swift -p 'break here' -X second")
        bp = target.breakpoint[-1]
        self.assertTrue(bp.IsEnabled())
        self.assertEqual(bp.GetNumResolvedLocations(), 1)
        self.assertIn("Scope.second", str(bp.location[0]))
