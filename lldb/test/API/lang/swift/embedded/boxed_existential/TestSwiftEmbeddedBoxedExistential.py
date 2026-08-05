import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEmbeddedBoxedExistential(TestBase):
    @skipUnlessDarwin
    @swiftTest
    @skipUnlessEmbeddedSwift
    def test(self):
        self.build()
        self.runCmd("setting set symbols.swift-enable-ast-context false")

        target, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        # A payload that does not fit inline is boxed on the heap. Its fields
        # start past the box's HeapObject header, not at the box address.
        self.expect(
            "frame variable pBoxed",
            substrs=["a.Boxed", "a = 111", "b = 222", "c = 333", "d = 444"],
        )
        self.expect(
            "frame variable pEnumLarge", substrs=["a.E", "large", "1", "2", "3", "4"]
        )

        # Payloads that do fit inline, and a class reference, which is one word
        # and is therefore never boxed.
        self.expect("frame variable pInline", substrs=["a.Inline", "a = 555", "b = 666"])
        self.expect("frame variable pEnumSmall", substrs=["a.E", "small", "888"])
        self.expect("frame variable pClass", substrs=["a.C", "classField = 777"])
        self.expect("frame variable classBound", substrs=["a.C", "classField = 777"])

        # A class-constrained existential is a reference plus a witness table,
        # not a five-word opaque container.
        self.expect(
            "frame variable -d no-dynamic-values classBound",
            substrs=["a.ClassBound", "object", "wtable"],
        )
        self.expect(
            "frame variable -d no-dynamic-values classBound",
            substrs=["payload_data_"],
            matching=False,
        )

        # A tuple payload is described only by DWARF, since it is not nominal
        # and embedded Swift emits stub metadata for it.
        self.expect(
            "frame variable boxedTuple",
            substrs=["(Int, Int, Int, Int)", "0 = 1", "1 = 2", "2 = 3", "3 = 4"],
        )
        self.expect(
            "frame variable inlineTuple", substrs=["(Int, Int)", "0 = 5", "1 = 6"]
        )

        # An error existential points to a heap box that names the payload's
        # type. The payload follows the box's header, metadata and witness
        # table, so a wrong offset here would print garbage rather than fail.
        self.expect(
            "frame variable anyError",
            substrs=["a.MyError", "code = 999", "extra = 111"],
        )

        # A class payload is a reference stored in that slot, so resolving it
        # has to step through to the instance.
        self.expect(
            "frame variable classError",
            substrs=["a.MyClassError", "classCode = 4242"],
        )
