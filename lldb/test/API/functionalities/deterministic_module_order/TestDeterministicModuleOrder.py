"""
Test that module loading order is deterministic.
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class DeterministicModuleOrderTestCase(TestBase):
    NO_DEBUG_INFO_TESTCASE = True

    def setUp(self):
        TestBase.setUp(self)

    def test_create_target_module_load_order(self):
        """Test that module loading order is deterministic across multiple runs."""

        self.build()
        exe = self.getBuildArtifact("a.out")

        # First run: Create target and save module list
        target = self.dbg.CreateTarget(exe)
        self.assertTrue(target, VALID_TARGET)

        first_run_modules = []
        for i in range(target.GetNumModules()):
            module = target.GetModuleAtIndex(i)
            module_name = module.GetFileSpec().GetFilename()
            first_run_modules.append(module_name)
        
        self.dbg.DeleteTarget(target)

        if self.TraceOn():
            print(f"First run module list: {first_run_modules}")

        if not first_run_modules:
            self.fail("No modules were found during the first run")

        # Subsequent runs: Create target and compare module list to first run
        num_additional_runs = 9  # Total of 10 runs including the first one
        for run in range(num_additional_runs):
            target = self.dbg.CreateTarget(exe)
            self.assertTrue(target, VALID_TARGET)
            current_modules = []
            for i in range(target.GetNumModules()):
                module = target.GetModuleAtIndex(i)
                module_name = module.GetFileSpec().GetFilename()
                current_modules.append(module_name)

            if self.TraceOn():
                print(f"Run {run+2} module list: {current_modules}")

            self.assertEqual(
                first_run_modules, 
                current_modules, 
                f"Module list in run {run+2} differs from the first run"
            )

            self.dbg.DeleteTarget(target)
