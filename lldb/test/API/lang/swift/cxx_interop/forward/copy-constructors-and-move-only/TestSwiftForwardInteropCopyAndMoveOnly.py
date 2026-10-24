"""
Test C++ types with non-trivial copy and destroy operations, and C++ types
that Swift imports as ~Copyable. Copying such a value into an expression
result has to call the C++ copy constructor, and ~Copyable values must never
be copied.
"""
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *


class TestSwiftForwardInteropCopyAndMoveOnly(TestBase):
    def launch(self):
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )

    def counter(self, name):
        value = self.target().FindFirstGlobalVariable(name)
        self.assertSuccess(value.GetError(), name)
        return value.GetValueAsSigned()

    def live_copy_counted(self):
        """The number of CopyCounted objects that have not been destroyed."""
        return (
            self.counter("numConstructed")
            + self.counter("numCopied")
            - self.counter("numDestroyed")
        )

    @swiftClangImporterTest
    def test_working(self):
        self.launch()
        self.expect_var_path("copyCounted.value", value="1")
        self.expect_var_path("hasMember.member.value", value="2")
        self.expect_var_path("hasMember.other", value="3")
        self.expect(
            "frame var virtualBase",
            patterns=[r"VirtualBase = \(baseValue = 20\)", r"ownValue = 21\b"],
        )
        self.expect_var_path("swiftStruct.member.value", value="4")
        self.expect_var_path("swiftStruct.number", value="42")
        self.expect_var_path("nonCopyable.x", value="5")
        self.expect_var_path("holder.member.x", value="6")
        self.expect("frame var derived", patterns=[r"NonCopyable = \(x = 7\)"])
        self.expect_var_path("annotated.x", value="8")

        # Copying into $R calls the copy constructor, and the expression
        # destroys its temporaries.
        live = self.live_copy_counted()
        copies = self.counter("numCopied")
        self.expect("expr copyCounted", substrs=["value = 1"])
        self.assertGreater(self.counter("numCopied"), copies)
        self.assertEqual(self.live_copy_counted(), live + 1)
        self.expect("expr var $c = copyCounted")
        self.assertEqual(self.live_copy_counted(), live + 2)
        self.expect_expr("$c.value", result_value="1")
        self.expect_expr("copyCounted.value", result_value="1")
        self.assertEqual(self.live_copy_counted(), live + 2)

        # The implicit copy constructors call CopyCounted's.
        self.expect("expr hasMember", substrs=["value = 2"])
        self.assertEqual(self.live_copy_counted(), live + 3)
        self.expect("expr swiftStruct", substrs=["value = 4"])
        self.assertEqual(self.live_copy_counted(), live + 4)
        self.expect_expr("hasMember.member.value", result_value="2")
        self.assertEqual(self.live_copy_counted(), live + 4)
        self.expect_expr("virtualBase.ownValue", result_value="21")

        # Reading a field of a ~Copyable value borrows it.
        self.expect_expr("nonCopyable.x", result_value="5")
        self.expect_expr("swiftNC.member.x", result_value="9")
        self.expect_var_path("nonCopyable.x", value="5")
        self.assertEqual(self.counter("numMoved"), 2)

    # Without ClangImporter, expressions can't see the C++ declarations
    # (rdar://106871275), and the types lose their non-copyability, so LLDB
    # calls the deleted copy constructor or copies an AnnotatedNonCopyable.
    @skipIf(swift_module_importer=["noclang"], bugnumber="rdar://106871275")
    @swiftClangImporterTest
    def test_not_working_in_noclang(self):
        self.launch()
        live = self.live_copy_counted()
        self.expect("expr CopyCounted(30)", substrs=["value = 30"])
        self.assertEqual(self.live_copy_counted(), live + 1)
        self.expect_expr("CopyCounted(31).value", result_value="31")
        self.assertEqual(self.live_copy_counted(), live + 1)
        # Assigning destroys the old value.
        self.expect("expr var $c = copyCounted")
        self.assertEqual(self.live_copy_counted(), live + 2)
        self.expect("expr $c = CopyCounted(32)")
        self.assertEqual(self.live_copy_counted(), live + 2)
        self.expect_expr("$c.value", result_value="32")

        # A ~Copyable value can't be the result. Moving it out of the program
        # would leave the variable in a moved-from state.
        for var in ["nonCopyable", "holder", "derived", "annotated", "swiftNC"]:
            self.expect(
                "expr " + var,
                error=True,
                substrs=["results in a ~Copyable type"],
            )
        self.expect_var_path("nonCopyable.x", value="5")
        self.assertEqual(self.counter("numMoved"), 2)

        self.expect_expr("holder.member.x", result_value="6")
        self.expect_expr("derived.x", result_value="7")
        self.expect_expr("classNC.member.x", result_value="10")
        self.expect("expr let $nc = NonCopyable(33)")
        self.expect_expr("$nc.x", result_value="33")
        self.expect_expr("NonCopyable(34).x", result_value="34")
        self.assertEqual(self.counter("numMoved"), 2)

    @swiftClangImporterTest
    def test_known_failures(self):
        self.launch()

        if False:  # rdar://190774376
            # Calling a method on a ~Copyable C++ value asserts in
            # DiagnoseStaticExclusivity.
            self.expect_expr("nonCopyable.method(3)", result_value="15")

        if False:  # rdar://190470866
            self.expect_var_path("derived.x", value="7")
            self.expect_var_path("virtualBase.baseValue", value="20")

        if False:  # rdar://177433184
            # Reflection can't resolve the ~Copyable field types. Embedded
            # Swift reads them from DWARF and works.
            self.expect_var_path("swiftNC.member.x", value="9")
            self.expect_var_path("swiftNC.number", value="43")
            self.expect_var_path("classNC.member.x", value="10")
            self.expect_var_path("classNC.number", value="44")

        if False:  # rdar://126944533
            self.expect("po nonCopyable", substrs=["x : 5"])

        if False:  # rdar://190774598
            # The diagnostic calls the persistent variable 'unknown'.
            self.expect("expr let $nc = NonCopyable(35)")
            self.expect(
                "expr let $moved = $nc",
                error=True,
                substrs=["'$nc' is borrowed and cannot be consumed"],
            )
