import lldb
import json
import os
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil

class TestCase(TestBase):

    NO_DEBUG_INFO_TESTCASE = True

    def get_stats(self):
        """
            Get the output of the "statistics dump" command and return the JSON
            as a python dictionary
        """
        return_obj = lldb.SBCommandReturnObject()
        command = "statistics dump"
        self.ci.HandleCommand(command, return_obj, False)
        metrics_json = return_obj.GetOutput()
        return json.loads(metrics_json)

    def force_module_load(self):
        """
            Force a module to load with expression evaluation
        """
        self.expect("po point", substrs=["{23, 42}"])

    @swiftTest
    @skipUnlessFoundation
    def test_type_system_info(self):
        """
            Test 'statistics dump' and the type system info
        """
        self.build()
        target = self.createTestTarget()
        lldbutil.run_to_source_breakpoint(self, "// break here",
                                          lldb.SBFileSpec("main.swift"))
        # Do something to cause modules to load
        self.force_module_load()

        debug_stats = self.get_stats()
        self.assertTrue("modules" in debug_stats)
        modules_array = debug_stats["modules"]
        for module in modules_array:
            if "TypeSystemInfo" in module:
                type_system_info = module["TypeSystemInfo"]
                self.assertTrue("swiftmodules" in type_system_info)
                for module_info in type_system_info["swiftmodules"]:
                    self.assertTrue("loadTime" in module_info)
                    self.assertTrue("name" in module_info)
