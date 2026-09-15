import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import TestBase
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):

    @requireNotEmbeddedSwift
    @skipUnlessPlatform(["macosx", "linux"])
    @swiftTest
    def test_task_tree_finds_unstructured_tasks(self):
        """An unstructured Task has no parent, child, or waiter edge to any
        running task, so only the task registry can discover it."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.runCmd("language swift task tree --max-frames 1")

        # (lldb) task tree --max-frames 1
        # ├╴ Task 1, addr = 0x... [running]
        # │    frame #0: Park.run() at main.swift:27:11
        # ├╴ Task 2 ('unstructured'), addr = 0x... [suspended]
        # │    frame #1: Park.forever(thenResume:) at main.swift:8:11
        # └╴ Task 3 ('detached'), addr = 0x... [suspended]
        #      frame #1: Park.forever(thenResume:) at main.swift:8:11

        lines = [line for line in self.res.GetOutput().splitlines() if line.strip()]
        self.assertEqual(len(lines), 6)

        # The top level task.
        self.assertIn("Task 1", lines[0])
        self.assertIn("[running]", lines[0])
        self.assertIn("Park.run()", lines[1])

        # Tasks that can only be found through the task registry.
        self.assertIn("('unstructured')", lines[2])
        self.assertIn("[suspended]", lines[2])
        self.assertIn("Park.forever(thenResume:)", lines[3])

        self.assertIn("('detached')", lines[4])
        self.assertIn("[suspended]", lines[4])
        self.assertIn("Park.forever(thenResume:)", lines[5])

    @skipUnlessEmbeddedSwift
    @skipEmbeddedSwiftOnLinux
    @skipUnlessPlatform(["macosx", "linux"])
    @swiftTest
    def test_task_tree_warns_without_registry(self):
        """The embedded concurrency runtime has no task registry, so `task tree`
        falls back to the edge walk and must warn that the list is partial."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.runCmd("language swift task tree --max-frames 1")

        self.assertIn(
            "warning: Task registry was not found in the concurrency runtime.",
            self.res.GetError(),
        )
        # Only the running task is reachable, so neither name shows up.
        output = self.res.GetOutput()
        self.assertNotIn("unstructured", output)
        self.assertNotIn("detached", output)
