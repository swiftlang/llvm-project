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
        # Task IDs follow creation order: 1 main, 2 unstructured, 3 a, 4 b,
        # 5-7 group children, 8 x, 9 y.
        self.runCmd("language swift task tree --max-frames 0")
        # LLDB draws the tree in ASCII unless LANG is UTF-8.
        output = self.res.GetOutput()
        for glyph, ascii in (("├╴ ", "|-"), ("└╴ ", "|-"), ("│", "|")):
            output = output.replace(glyph, ascii)
        tree_file = self.getBuildArtifact("task_tree.txt")
        with open(tree_file, "w") as f:
            f.write(output)
        self.filecheck_log(tree_file, __file__)
        # CHECK:      {{^}}|-Task 1, addr = 0x{{[0-9a-f]+}} [awaiting Task 2] [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |-Task 5, addr = 0x{{[0-9a-f]+}} [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |-Task 6, addr = 0x{{[0-9a-f]+}} [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |-Task 7, addr = 0x{{[0-9a-f]+}} [awaiting Task 2] [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |-Task 4, addr = 0x{{[0-9a-f]+}} [awaiting Task 8] [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |  |-Task 9, addr = 0x{{[0-9a-f]+}} [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |  |-Task 8, addr = 0x{{[0-9a-f]+}} [suspended]{{$}}
        # CHECK-NEXT: {{^}}|  |-Task 3, addr = 0x{{[0-9a-f]+}} [suspended]{{$}}
        # CHECK-NEXT: {{^}}|-Task 2, addr = 0x{{[0-9a-f]+}} [running]{{$}}
        # CHECK-NOT:  Task
