import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


@skipIf(archs=["x86_64"], bugnumber="rdar://174750739")
@skipIfDarwinEmbedded
class TestCase(TestBase):

    @requireNotEmbeddedSwift
    @requireDarwin
    @swiftTest
    def test_body(self):
        self.build()
        self.thread = self._run_to_source_breakpoint("break body")
        self._do_test("self._count", 41, is_graph_update=True)

    @requireNotEmbeddedSwift
    @requireDarwin
    @swiftTest
    def test_appear(self):
        self.build()
        log = self.getBuildArtifact("types.log")
        self.expect(f"log enable lldb types -v -f {log}")
        self.thread = self._run_to_source_breakpoint("break appear")
        self._do_test("self._count", 41, is_graph_update=False)

    @requireNotEmbeddedSwift
    @requireDarwin
    @swiftTest
    def test_change(self):
        self.build()
        log = self.getBuildArtifact("types.log")
        self.expect(f"log enable lldb types -v -f {log}")
        self.thread = self._run_to_source_breakpoint("break change")
        # Assignment in onAppear
        self._do_test("self._count", 23, is_graph_update=False)
        self.thread.process.Continue()
        # Callback in onChange
        self._do_test("self._count", 23, is_graph_update=False)

    def _run_to_source_breakpoint(self, source_regex: str) -> lldb.SBThread:
        target = self.dbg.CreateTarget(self.getBuildArtifact("a.out"))
        self.assertTrue(target, VALID_TARGET)
        # SwiftUI.State internally contains a noncopyable data type whose
        # reflection metadata is missing if the stdlib is compiled with a
        # deployment target < macOS 27.0, which is the case when building
        # locally. Debug against the system Swift stdlib (whose deployment
        # target matches the OS) by first dropping the library-path variables
        # the test harness injects.
        for var in (
            "DYLD_LIBRARY_PATH",
            "LD_LIBRARY_PATH",
            "SIMCTL_CHILD_DYLD_LIBRARY_PATH",
        ):
            self.runCmd(f"settings remove target.env-vars {var}", check=False)
        bkpt = target.BreakpointCreateBySourceRegex(
            source_regex, lldb.SBFileSpec("main.swift")
        )
        self.assertGreater(bkpt.GetNumLocations(), 0)
        _, _, thread, _ = lldbutil.run_to_breakpoint_do_run(self, target, bkpt)
        return thread

    def _do_test(self, var_name: str, value: int, *, is_graph_update: bool):
        symbol = "AG::Graph::UpdateStack::update()"
        if is_graph_update:
            self.assertIn(symbol, (f.name for f in self.thread))
        else:
            self.assertNotIn(symbol, (f.name for f in self.thread))

        frame = self.thread.selected_frame
        count = frame.var(var_name)
        self.assertEqual(count.GetNumChildren(), 1)
        self.assertEqual(count.member["wrappedValue"].unsigned, value)
        self.assertEqual(count.summary, str(value))
