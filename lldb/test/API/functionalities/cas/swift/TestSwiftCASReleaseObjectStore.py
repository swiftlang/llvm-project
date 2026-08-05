"""
Test that a Swift debug session releases its CAS object store when the debugger
is destroyed, and gives up as little as it can to do so: a SwiftASTContext is
rebuilt on demand, an lldb Module's symbol table and DWARF index are not.
"""

import re

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftCASReleaseObjectStore(TestBase):
    def debug_in_a_fresh_debugger(self, executable, log_path):
        """Debug `executable` in a debugger of our own -- the test's primary
        debugger would keep the modules alive -- evaluate an expression that
        needs the CAS-backed Swift module, then destroy that debugger. Returns
        the log contents."""
        debugger = lldb.SBDebugger.Create()
        try:
            debugger.SetAsync(False)
            result = lldb.SBCommandReturnObject()
            debugger.GetCommandInterpreter().HandleCommand(
                'log enable lldb module -f "%s"' % log_path, result
            )
            self.assertTrue(result.Succeeded(), result.GetError())

            target = debugger.CreateTarget(executable)
            self.assertTrue(target.IsValid())

            breakpoint = target.BreakpointCreateBySourceRegex(
                "break here", lldb.SBFileSpec("main.swift")
            )
            self.assertEqual(breakpoint.GetNumLocations(), 1)

            process = target.LaunchSimple(
                None, None, self.get_process_working_directory()
            )
            self.assertState(process.GetState(), lldb.eStateStopped)

            frame = process.GetSelectedThread().GetFrameAtIndex(0)
            value = frame.EvaluateExpression("obj")
            self.assertSuccess(value.GetError(), "evaluating 'obj'")
            self.assertIn("x = 0", str(value))

            # The Swift module was hidden from the filesystem, so the context
            # that just answered this can only have loaded it out of the CAS.
            self.assertTrue(
                frame.GetLanguageSpecificData()
                .GetValueForKey("SwiftHasCAS")
                .GetBooleanValue()
            )
        finally:
            lldb.SBDebugger.Destroy(debugger)

        with open(log_path, "r") as f:
            return f.read()

    def assertModuleNotConstructed(self, log_contents, path, msg=None):
        pattern = r"Module::Module\(\(.*?\) '%s'\)" % re.escape(path)
        self.assertNotRegex(log_contents, pattern, msg)

    def countModulesConstructed(self, log_contents):
        return len(re.findall(r"Module::Module\(", log_contents))

    @skipEmbeddedSwift
    # Don't run ClangImporter tests if ClangImporter is disabled.
    @skipIf(setting=("symbols.use-swift-clangimporter", "false"))
    @skipIf(setting=("symbols.swift-precise-compiler-invocation", "false"))
    @skipUnlessDarwin
    @swiftTest
    def test_release_on_debugger_destroy(self):
        """Destroying a debugger releases the CAS its SwiftASTContext loaded
        modules out of, and debugging the same binary again evaluates the same
        expression out of a store that has to be instantiated again."""
        self.build()
        executable = self.getBuildArtifact("a.out")

        first_log = self.debug_in_a_fresh_debugger(
            executable, self.getBuildArtifact("first_session.log")
        )
        self.assertIn("Released CAS at", first_log)
        self.assertIn("0 still referenced", first_log)

        # How much this costs depends on the debug info format. With a dSYM,
        # dsymutil has resolved the -gmodules references, so the store is only
        # held by the SwiftASTContext and Module::m_cas, and nothing at all has
        # to be evicted. Without one, main.swift.o is read through a debug map
        # and its DWARF still refers to the clang modules of the explicit module
        # build by CASID, so those are read out of the store and have to go --
        # but nothing imported clang types out of them, so main.swift.o gives
        # them up and it and the executable are still kept.
        if self.getDebugInfo() == "dsym":
            self.assertRegex(
                first_log,
                r"Released object stores: [1-9][0-9]* module\(s\) kept, "
                r"0 module\(s\) removed,",
            )
        else:
            self.assertIn("Loading module 'ClangA' from CAS at", first_log)
            self.assertRegex(
                first_log,
                r"Released object stores: [1-9][0-9]* module\(s\) kept, "
                r"[1-9][0-9]* module\(s\) removed,",
            )

        # Debug the same binary again. The expression has to produce the same
        # answer out of a CAS that has to be instantiated again -- both asserted
        # in debug_in_a_fresh_debugger().
        second_log = self.debug_in_a_fresh_debugger(
            executable, self.getBuildArtifact("second_session.log")
        )
        self.assertIn("Initialized CAS at", second_log)

        # Whatever was not removed is reused rather than rebuilt: no ObjectFile,
        # symbol table or DWARF index is reconstructed for it. Compared by order
        # of magnitude, since the exact count depends on what the OS loads.
        first_modules = self.countModulesConstructed(first_log)
        second_modules = self.countModulesConstructed(second_log)
        self.assertGreater(first_modules, 10, "first session built the world")
        self.assertLess(
            second_modules,
            first_modules // 10,
            "second session rebuilt %d of the first session's %d module(s)"
            % (second_modules, first_modules),
        )
        # The executable is kept in both formats, so it is never rebuilt.
        self.assertModuleNotConstructed(second_log, executable)
