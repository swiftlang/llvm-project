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

    @skipIfLinux  # Crashes.
    @skipIfWindows
    @swiftClangImporterTest
    def test_frame_variable(self):
        self.launch()
        self.expect("frame var shared", substrs=["(SharedFRT) shared"])
        self.check_var("shared.value", 1)
        self.check_var("immortal.value", 2)
        self.check_var("unsafe.value", 3)
        # Like class references, FRTs display as pointers.
        self.expect(
            "frame var maybeShared",
            patterns=[r"\(SharedFRT\?\) maybeShared = 0x[0-9a-f]+", r"value = 4\b"],
        )
        if not self.isEmbeddedSwift():  # rdar://190470875
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

    @skipIfLinux  # Crashes.
    @skipIfWindows
    @swiftClangImporterTest
    def test_generic_parameter(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break in generic", lldb.SBFileSpec("main.swift")
        )
        # An FRT has no Swift metadata, so its bound type is its dynamic type.
        self.expect(
            "frame var -d run-target t",
            patterns=[r"\(SharedFRT\) t = 0x[0-9a-f]+", r"value = 1\b"],
        )

    @skipIfLinux  # Crashes.
    @skipIfWindows
    # Expressions without ClangImporter can't see SharedFRT.create().
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://106871275")
    @swiftClangImporterTest
    def test_expr_retain_release(self):
        self.launch()
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

    @skipIfLinux  # Crashes.
    @skipIfWindows
    # Expressions without ClangImporter can't see SharedFRT.create().
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://106871275")
    @swiftClangImporterTest
    def test_expr_result_display(self):
        self.launch()
        self.expect("expr SharedFRT.create(16)", patterns=[r"value = 16\b"])
        self.expect("expr let $frt = SharedFRT.create(17)")
        self.expect("expr $frt", patterns=[r"value = 17\b"])

    @skipIfLinux  # Crashes.
    @skipIfWindows
    # Without ClangImporter, expressions treat FRTs as value types.
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://190470881")
    @swiftClangImporterTest
    def test_expr_local_variables(self):
        self.launch()
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

    @skipIfLinux  # Crashes.
    @skipIfWindows
    @requireNotEmbeddedSwift
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://190470881")
    @swiftClangImporterTest
    def test_po_local_variables(self):
        self.launch()
        # The runtime describes an FRT without its fields.
        self.expect("po shared", substrs=["<SharedFRT: "])
        self.expect("po immortal", substrs=["<ImmortalFRT: "])

    @skipIfLinux  # Crashes.
    @skipIfWindows
    # Expressions without ClangImporter can't see C++ methods and functions.
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://190470989")
    @swiftClangImporterTest
    def test_expr_calls_with_local_variables(self):
        self.launch()
        self.expect_expr("shared.getValue()", result_value="1")
        self.expect_expr("getIncompleteFRTValue(incomplete)", result_value="12")
        self.expect_expr("asSharedFRT(nonPrimary).value", result_value="11")

    @skipIf(bugnumber="rdar://190470866+190470885+190470954")
    @swiftClangImporterTest
    def test_known_failures(self):
        if False:  # rdar://190470866
            self.check_var("derived.value", 8)
            self.check_var("nonPrimary.firstValue", 10)
            self.check_var("nonPrimary.value", 11)

        if False:  # rdar://190470885, rdar://190470954 (Embedded Swift)
            for var in ["structWithFRTs", "classWithFRTs"]:
                self.check_var(var + ".shared.value", 1)
                self.expect(
                    "frame var %s.maybeShared" % var,
                    substrs=["some"],
                    patterns=[r"value = 1\b"],
                )
                self.check_var(var + ".immortal.value", 2)
            self.check_var("structWithFRTs.virtualRC.value", 8)
            self.check_var("structWithFRTs.number", 42)
            self.check_var("classWithFRTs.number", 43)
