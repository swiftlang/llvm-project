# TestSwiftStaticArchiveTwoSwiftmodules.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2018 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil

class TestSwiftStaticArchiveTwoSwiftmodules(TestBase):
    @requireNotEmbeddedSwift
    # Don't run ClangImporter tests if Clangimporter is disabled.
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false'))
    @requireDarwin
    @swiftTest
    def test(self):
        self.build()
        _, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('Foo.swift'))

        # This test tests that the search paths from all swiftmodules
        # that are part of the main binary are honored.
        self.expect("fr var foo", "expected result", substrs=["23"])
        self.expect("expression foo", "expected result", substrs=["$R0", "i", "23"])
        lldbutil.continue_to_source_breakpoint(
            self, process, 'break here', lldb.SBFileSpec('Bar.swift'))
        self.expect("fr var bar", "expected result", substrs=["42"])
        self.expect("expression bar", "expected result", substrs=["j", "42"])
