"""
Test foreign reference types: C++ types that Swift imports as reference types
via SWIFT_SHARED_REFERENCE, SWIFT_IMMORTAL_REFERENCE, or SWIFT_UNSAFE_REFERENCE.
Unlike Swift classes, they have no isa pointer and no Swift metadata.
"""
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftForwardInteropForeignReferenceTypes(TestBase):
    def launch(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

    def var(self, path, use_dynamic=lldb.eNoDynamicValues):
        value = self.frame().GetValueForVariablePath(path, use_dynamic)
        self.assertSuccess(value.GetError(), path)
        return value.GetValueAsSigned()

    def check_var(self, path, expected, use_dynamic=lldb.eNoDynamicValues):
        self.assertEqual(self.var(path, use_dynamic), expected, path)

    def last_created_refcount(self):
        frt = self.target().FindFirstGlobalVariable("lastCreatedSharedFRT")
        refcount = frt.Dereference().GetChildMemberWithName("refCount")
        self.assertSuccess(refcount.GetError())
        return refcount.GetValueAsSigned()

    def check_frame_variable(self):
        self.expect("frame var shared", substrs=["(SharedFRT) shared"])
        self.check_var("shared.value", 1)
        self.check_var("immortal.value", 2)
        self.check_var("unsafe.value", 3)
        self.expect("frame var virtualBase", patterns=[r"baseValue = 20\b"])
        self.check_var("virtualBase.baseValue", 20)
        # Like class references, FRTs display as pointers.
        self.expect(
            "frame var maybeShared",
            patterns=[r"\(SharedFRT\?\) maybeShared = 0x[0-9a-f]+", r"value = 4\b"],
        )
        self.expect("frame var noShared", substrs=["noShared = nil"])
        self.check_var("member.value", 6)
        self.check_var("virtualRC.value", 7)
        self.expect(
            "frame var derived",
            patterns=[
                r"VirtualRefCounted = \(value = 8, refCount = \d+\)",
                r"derivedValue = 9\b",
            ],
        )
        self.check_var("derived.value", 8)
        # Dynamic type resolution must not read the C++ vtable pointer as an
        # isa pointer.
        self.check_var("upcast.value", 8, lldb.eDynamicCanRunTarget)
        self.check_var("derived.derivedValue", 9, lldb.eDynamicCanRunTarget)
        self.expect(
            "frame var nonPrimary",
            patterns=[
                r"FirstBase = \(firstValue = 10\)",
                r"SharedFRT = \(value = 11, refCount = \d+\)",
                r"ownValue = 30\b",
            ],
        )
        self.check_var("nonPrimary.firstValue", 10)
        self.check_var("nonPrimary.value", 11)
        # asSharedFRT() adjusted the pointer to the non-primary base.
        self.check_var("nonPrimaryBase.value", 11)
        # Only impl.cpp's debug info has the definition.
        self.check_var("incomplete.value", 12)
        # No debug info has the definition.
        self.expect(
            "frame var opaque",
            substrs=["Could not find reflection metadata"],
            matching=False,
        )
        # Swift only uses this type as an Optional.
        self.expect(
            "frame var optionalOnly",
            patterns=[r"optionalOnly = 0x[0-9a-f]+", r"value = 16\b"],
        )
        # A C++ pointer to an FRT is a reference.
        self.check_var("holder.shared.value", 1)

    def check_fields(self):
        for var in ["structWithFRTs", "classWithFRTs"]:
            self.check_var(var + ".shared.value", 1)
            self.expect(
                "frame var %s.maybeShared" % var,
                patterns=[r"maybeShared = 0x[0-9a-f]+", r"value = 1\b"],
            )
            self.check_var(var + ".immortal.value", 2)
        self.check_var("structWithFRTs.virtualRC.value", 8)
        self.check_var("structWithFRTs.number", 42)
        self.check_var("classWithFRTs.number", 43)

    def check_generic_parameter(self):
        lldbutil.continue_to_source_breakpoint(
            self, self.process(), "break in generic", lldb.SBFileSpec("main.swift")
        )
        # An FRT has no Swift metadata, so its bound type is its dynamic type.
        self.expect(
            "frame var -d run-target t",
            patterns=[r"\(SharedFRT\) t = 0x[0-9a-f]+", r"value = 1\b"],
        )

    def check_expr_retain_release(self):
        shared = self.var("shared.refCount")
        # upcast is the same object as derived.
        derived = self.var("upcast.refCount")
        # An expression that keeps no reference releases what it retains.
        self.expect_expr("structWithFRTs.shared.value", result_value="1")
        self.expect_expr("structWithFRTs.virtualRC.value", result_value="8")
        self.check_var("shared.refCount", shared)
        self.check_var("upcast.refCount", derived)

        # $R holds its own references. The virtual retain() dispatches to the
        # override.
        derived_retains = self.var("derived.derivedRetains")
        self.expect("expr structWithFRTs")
        self.check_var("shared.refCount", shared + 2)
        self.check_var("upcast.refCount", derived + 1)
        self.check_var("derived.derivedRetains", derived_retains + 1)

        # A persistent variable takes ownership of a returned-retained object.
        self.expect("expr let $frt = SharedFRT.create(13)")
        self.assertEqual(self.last_created_refcount(), 1)
        self.expect_expr("$frt.value", result_value="13")
        self.assertEqual(self.last_created_refcount(), 1)

        self.expect_expr("SharedFRT.create(14).value", result_value="14")
        self.assertEqual(self.last_created_refcount(), 0)

        self.expect("expr var $maybeFRT: SharedFRT? = SharedFRT.create(15)")
        self.assertEqual(self.last_created_refcount(), 1)
        self.expect("expr $maybeFRT = nil")
        self.assertEqual(self.last_created_refcount(), 0)

    def check_expr_result_display(self):
        self.expect("expr SharedFRT.create(16)", patterns=[r"value = 16\b"])
        self.expect("expr let $shown = SharedFRT.create(17)")
        self.expect("expr $shown", patterns=[r"value = 17\b"])

    def check_expr_local_variables(self):
        refcount = self.var("shared.refCount")
        self.expect_expr("shared.value", result_value="1")
        self.expect_expr("immortal.value", result_value="2")
        self.expect_expr("unsafe.value", result_value="3")
        self.expect_expr("maybeShared!.value", result_value="4")
        self.expect_expr("noShared == nil", result_summary="true")
        self.expect_expr("member.value", result_value="6")
        self.expect_expr("virtualRC.value", result_value="7")
        self.expect_expr("derived.derivedValue", result_value="9")
        self.expect_expr("upcast.value", result_value="8")
        self.expect_expr("nonPrimary.value", result_value="11")
        self.expect_expr("nonPrimaryBase.value", result_value="11")
        self.expect("expr opaque", substrs=["OpaqueFRT"])
        self.expect_expr("optionalOnly!.value", result_value="16")
        self.check_var("shared.refCount", refcount)
        # Assigning to a variable releases the reference it held.
        self.expect("expr reassigned = nil")
        self.expect_expr("reassigned == nil", result_summary="true")
        self.check_var("shared.refCount", refcount - 1)

    def check_po_local_variables(self):
        # The runtime describes an FRT without its fields.
        self.expect("po shared", substrs=["<SharedFRT: "])
        self.expect("po immortal", substrs=["<ImmortalFRT: "])

    def check_expr_calls_with_local_variables(self):
        self.expect_expr("shared.getValue()", result_value="1")
        self.expect_expr("getIncompleteFRTValue(incomplete)", result_value="12")
        self.expect_expr("asSharedFRT(nonPrimary).value", result_value="11")

    # Each test runs its checks in one process, because launching dominates the
    # run time.
    @skipIfLinux  # Crashes.
    @skipIfWindows
    @swiftClangImporterTest
    def test_frame_variable(self):
        self.launch()
        self.check_frame_variable()
        # Outside Embedded Swift, reflection records FRT fields as
        # OpaquePointer (rdar://190470885).
        if self.isEmbeddedSwift():
            self.check_fields()
        self.check_generic_parameter()

    @skipIfLinux  # Crashes.
    @skipIfWindows
    # Without ClangImporter, expressions treat FRTs as value types
    # (rdar://190470881) and can't see C++ functions and methods
    # (rdar://106871275, rdar://190470989).
    @skipIf(
        swift_module_importer=["noclang"],
        bugnumber="rdar://190470881+106871275+190470989",
    )
    @swiftClangImporterTest
    def test_expr(self):
        self.launch()
        self.check_expr_local_variables()
        self.check_expr_calls_with_local_variables()
        if not self.isEmbeddedSwift():
            self.check_po_local_variables()
        self.check_expr_result_display()
        self.check_expr_retain_release()
