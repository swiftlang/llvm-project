import re
import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import TestBase
import lldbsuite.test.lldbutil as lldbutil


class TestCase(TestBase):

    @skipUnlessEmbeddedSwift
    @skipEmbeddedSwiftOnLinux
    @skipUnlessPlatform(["macosx", "linux"])
    @swiftTest
    def test_task_tree_edges(self):
        """Without a task registry, `task tree` finds tasks only through the
        parent, child and waiter edges reachable from the running task."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        self.runCmd("language swift task tree --max-frames 0")
        output = self.res.GetOutput()

        # Task IDs follow creation order: 1 main, 2 unstructured, 3 a, 4 b,
        # 5-7 group children, 8 x, 9 y.
        tasks = []
        parent_of = {}
        ids = []
        for line in output.splitlines():
            match = re.search(r"Task (\d+), addr = ", line)
            if not match:
                continue
            column, task_id = match.start(), int(match.group(1))
            while tasks and tasks[-1][0] >= column:
                tasks.pop()
            parent_of[task_id] = tasks[-1][1] if tasks else None
            tasks.append((column, task_id))
            ids.append(task_id)

        self.assertEqual(sorted(ids), list(range(1, 10)), output)
        self.assertNotIn("no information available", output)
        self.assertEqual(
            parent_of,
            {1: None, 2: None, 3: 1, 4: 1, 5: 1, 6: 1, 7: 1, 8: 4, 9: 4},
            output,
        )
        self.assertRegex(output, r"Task 1, .*\[awaiting Task 2\]")
        self.assertRegex(output, r"Task 2, .*\[running\]")
        self.assertRegex(output, r"Task 4, .*\[awaiting Task 8\]")
        self.assertRegex(output, r"Task 7, .*\[awaiting Task 2\]")
