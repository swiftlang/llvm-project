import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil

class TestSwiftOptionalErrorHandling(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    @requireNotEmbeddedSwift
    @skipIf(swift_module_importer=no_match("noclang"))
    @swiftTest
    def test(self):
        """Test that errors are surfaced"""
        self.build()
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift"),
            extra_images=['Library'])
        self.expect('frame variable x', substrs=[
            'opaqueSome', 'missing debug info for Clang type', 'FromC',
            'opaqueNone', 'nil',
        ])
        # The bridging header imports no Clang modules, so opt out of the
        # check that noclang is only used for programs that need it.
        self.runCmd("settings set symbols.swift-validate-typesystem false")
