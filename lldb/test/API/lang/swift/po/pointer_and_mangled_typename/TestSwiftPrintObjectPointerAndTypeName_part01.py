import lldb
import lldbsuite.test.lldbutil as lldbutil
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestCase(TestBase):

    def setUp(self):
        TestBase.setUp(self)

        self.log = self.getBuildArtifact("expr.log")
        self.runCmd(f"log enable lldb expr -f {self.log}")
        self.inferior = None

    def _filecheck(self, key):
        self.filecheck_log(self.log, __file__, f"-check-prefix=CHECK-{key}")

    # main.swift runs each scenario in its own scope, in source order, so one
    # process visits them all. Each breakpoint is deleted once hit because
    # some patterns are prefixes of others ("break class-only protocol").
    def run_to(self, bkpt_pattern):
        src = lldb.SBFileSpec("main.swift")
        if not self.inferior:
            target, self.inferior, _, bkpt = lldbutil.run_to_source_breakpoint(
                self, bkpt_pattern, src
            )
            target.BreakpointDelete(bkpt.GetID())
        else:
            threads = lldbutil.continue_to_source_breakpoint(
                self, self.inferior, bkpt_pattern, src
            )
            self.assertEqual(len(threads), 1, f"no stop at {bkpt_pattern}")

    @swiftTest
    @skipEmbeddedSwiftOnWindows
    def test_stdlib_types(self):
        self.build()
        self.run_to("break int")
        self.expect("po value", substrs=["2025"])
        self._filecheck("INT")
        # CHECK-INT: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "SiD")

        self.run_to("break string")
        self.expect("po value", substrs=["Po"])
        self._filecheck("STRING")
        # CHECK-STRING: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "SSD")

    @skipEmbeddedSwift # rdar://184868750 (Embedded Swift: three po test files carry a byte-identical 30-line expectation helper)
    @swiftTest
    def test_user_types(self):
        self.build()
        self.run_to("break struct")
        self.expect("po value", substrs=["▿ Struct"])
        self._filecheck("STRUCT")
        # CHECK-STRUCT: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "1a6StructVD")

        self.run_to("break class")
        self.expect("po value", substrs=["<Class: 0x"])
        self._filecheck("CLASS")
        # CHECK-CLASS: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "1a5ClassCD")
