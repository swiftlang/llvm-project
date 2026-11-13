import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftResiliencePrivateField(lldbtest.TestBase):

    mydir = lldbtest.TestBase.compute_mydir(__file__)
    # The test methods use different build settings.
    SHARED_BUILD_TESTCASE = False

    def check(self):
        target, process, thread, bkpt = lldbutil.run_to_source_breakpoint(
            self, 'break here', lldb.SBFileSpec('main.swift'))

        self.expect("frame variable -- self.m", substrs=['42'])
        self.expect("expression -- m", substrs=['42'])

    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test(self):
        self.build()
        self.check()

    @requireNotEmbeddedSwift
    @swiftTest
    @expectedFailureAll(debug_info=["pdb"])
    def test_no_explicit_module_loader(self):
        """Test modules loaded from Swift AST sections."""
        self.build(dictionary={"SWIFT_ENABLE_EXPLICIT_MODULES": "NO"})
        self.runCmd(
            "settings set symbols.use-swift-explicit-module-loader false")
        self.check()
