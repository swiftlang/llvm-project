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

    @skipEmbeddedSwift # rdar://184868750 (Embedded Swift: three po test files carry a byte-identical 30-line expectation helper)
    @swiftTest
    def test(self):
        self.build()
        self.run_to("break enum")
        self.expect("po value", substrs=["▿ Enum"])
        self._filecheck("ENUM")
        # CHECK-ENUM: stringForPrintObject(UnsafeRawPointer(bitPattern: {{.*}}), mangledTypeName: "1a4EnumOD")

        self.run_to("break generic struct")
        self.expect("po value", substrs=["▿ GenericStruct<String>"])
        self._filecheck("GEN-STRUCT")
        # CHECK-GEN-STRUCT: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "1a13GenericStructVySSGD")

        self.run_to("break generic class")
        self.expect("po value", substrs=["<GenericClass<String>: 0x"])
        self._filecheck("GEN-CLASS")
        # CHECK-GEN-CLASS: stringForPrintObject(UnsafeRawPointer(bitPattern: {{[0-9]+}}), mangledTypeName: "1a12GenericClassCySSGD")

        self.run_to("break generic enum")
        self.expect("po value", substrs=["▿ GenericEnum<String>"])
        self._filecheck("GEN-ENUM")
        # CHECK-GEN-ENUM: stringForPrintObject(UnsafeRawPointer(bitPattern: {{.*}}), mangledTypeName: "1a11GenericEnumOySSGD")
