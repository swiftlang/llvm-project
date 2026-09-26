"""
Test lldb data formatter subsystem for std::span
"""

import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
from lldbsuite.test import lldbutil


class StdSpanDataFormatterTestCase(TestBase):
    TEST_WITH_PDB_DEBUG_INFO = True
    SHARED_BUILD_TESTCASE = False

    def findVariable(self, name):
        var = self.frame().FindVariable(name)
        self.assertTrue(var.IsValid())
        return var

    def dump_span_layout(self, var):
        """Print the on-target layout of a std::span.

        The MSVC STL synthetic provider finds the element count either in a
        `_Mysize` member or in a `_Mysize` constant on the extent base class.
        When neither resolves it reports an empty span, and the only way to tell
        what the STL actually named them is to read the type back off the
        target, so dump enough of it to be diagnosable from a CI log alone.
        """
        out = []
        try:
            type_name = var.GetTypeName()
            out.append(f"type={type_name}")
            out.append(f"num_children(synthetic)={var.GetNumChildren()}")

            raw = var.GetNonSyntheticValue()
            out.append(f"num_children(raw)={raw.GetNumChildren()}")
            for i in range(raw.GetNumChildren()):
                child = raw.GetChildAtIndex(i)
                out.append(
                    f"  raw child[{i}] name={child.GetName()!r} "
                    f"type={child.GetTypeName()!r} value={child.GetValue()!r}"
                )

            def dump_type(t, indent):
                pad = " " * indent
                out.append(
                    f"{pad}type={t.GetName()!r} fields={t.GetNumberOfFields()} "
                    f"bases={t.GetNumberOfDirectBaseClasses()}"
                )
                for i in range(t.GetNumberOfFields()):
                    f = t.GetFieldAtIndex(i)
                    out.append(
                        f"{pad}  field[{i}] name={f.GetName()!r} "
                        f"type={f.GetType().GetName()!r}"
                    )
                for i in range(t.GetNumberOfDirectBaseClasses()):
                    base = t.GetDirectBaseClassAtIndex(i).GetType()
                    out.append(f"{pad}  base[{i}]:")
                    dump_type(base, indent + 4)

            dump_type(raw.GetType(), 0)

            interp = self.dbg.GetCommandInterpreter()
            for cmd in (
                f"type lookup {type_name}",
                f"frame variable --raw {var.GetName()}",
            ):
                res = lldb.SBCommandReturnObject()
                interp.HandleCommand(cmd, res)
                out.append(f"--- {cmd} ---")
                out.append(res.GetOutput() or res.GetError() or "<no output>")
        except Exception as e:
            out.append(f"<dump failed: {e!r}>")

        print("### std::span layout dump ###")
        print("\n".join(out), flush=True)

    def check_size(self, var_name, size):
        var = self.findVariable(var_name)
        if var.GetNumChildren() != size:
            self.dump_span_layout(var)
        self.assertEqual(var.GetNumChildren(), size)

    def check_numbers(self, var_name):
        """Helper to check that data formatter sees contents of std::span correctly"""

        expectedSize = 5
        self.check_size(var_name, expectedSize)

        self.expect_expr(
            var_name,
            result_type=f"std::span<int, {expectedSize}>",
            result_summary=f"size={expectedSize}",
            result_children=[
                ValueCheck(name="[0]", value="1"),
                ValueCheck(name="[1]", value="12"),
                ValueCheck(name="[2]", value="123"),
                ValueCheck(name="[3]", value="1234"),
                ValueCheck(name="[4]", value="12345"),
            ],
        )

        # check access-by-index
        self.expect_var_path(f"{var_name}[0]", type="int", value="1")
        self.expect_var_path(f"{var_name}[1]", type="int", value="12")
        self.expect_var_path(f"{var_name}[2]", type="int", value="123")
        self.expect_var_path(f"{var_name}[3]", type="int", value="1234")
        self.expect_var_path(f"{var_name}[4]", type="int", value="12345")

    def do_test(self):
        """Test that std::span variables are formatted correctly when printed."""
        (self.target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.cpp", False)
        )

        lldbutil.continue_to_breakpoint(process, bkpt)

        # std::span of std::array with extents known at compile-time
        self.check_numbers("numbers_span")

        # check access to synthetic children for static spans
        self.runCmd(
            'type summary add --summary-string "item 0 is ${var[0]}" -x "std::span<" span'
        )
        self.expect_expr(
            "numbers_span",
            result_type="std::span<int, 5>",
            result_summary="item 0 is 1",
        )

        self.runCmd(
            'type summary add --summary-string "item 0 is ${svar[0]}" -x "std::span<" span'
        )
        self.expect_expr(
            "numbers_span",
            result_type="std::span<int, 5>",
            result_summary="item 0 is 1",
        )

        self.runCmd("type summary clear")

        # New span with strings
        lldbutil.continue_to_breakpoint(process, bkpt)

        expectedStringSpanChildren = [
            ValueCheck(name="[0]", summary='"smart"'),
            ValueCheck(name="[1]", summary='"!!!"'),
        ]

        self.expect_var_path(
            "strings_span", summary="size=2", children=expectedStringSpanChildren
        )

        # check access to synthetic children for dynamic spans
        dynamic_string_span = (
            "std::span<std::basic_string<char, std::char_traits<char>, std::allocator<char>>, -1>"
            if self.getDebugInfo() == "pdb"
            else "dynamic_string_span"
        )
        self.runCmd(
            f'type summary add --summary-string "item 0 is ${{var[0]}}" "{dynamic_string_span}"'
        )
        self.expect_var_path("strings_span", summary='item 0 is "smart"')

        self.runCmd(
            f'type summary add --summary-string "item 0 is ${{svar[0]}}" "{dynamic_string_span}"'
        )
        self.expect_var_path("strings_span", summary='item 0 is "smart"')

        self.runCmd(f'type summary delete "{dynamic_string_span}"')

        # test summaries based on synthetic children
        self.runCmd(
            f'type summary add --summary-string "span has ${{svar%#}} items" -e "{dynamic_string_span}"'
        )

        self.expect_var_path("strings_span", summary="span has 2 items")

        self.expect_var_path(
            "strings_span",
            summary="span has 2 items",
            children=expectedStringSpanChildren,
        )

        # check access-by-index
        self.expect_var_path("strings_span[0]", summary='"smart"')
        self.expect_var_path("strings_span[1]", summary='"!!!"')

        # Newly inserted value not visible to span
        lldbutil.continue_to_breakpoint(process, bkpt)

        self.expect_expr(
            "strings_span",
            result_summary="span has 2 items",
            result_children=expectedStringSpanChildren,
        )

        self.runCmd(f'type summary delete "{dynamic_string_span}"')

        lldbutil.continue_to_breakpoint(process, bkpt)

        # Empty spans
        self.expect_expr(
            "static_zero_span", result_type="std::span<int, 0>", result_summary="size=0"
        )
        self.check_size("static_zero_span", 0)

        self.expect_expr("dynamic_zero_span", result_summary="size=0")
        self.check_size("dynamic_zero_span", 0)

        # Nested spans
        self.expect_expr(
            "nested",
            result_summary="size=2",
            result_children=[
                ValueCheck(
                    name="[0]", summary="size=2", children=expectedStringSpanChildren
                ),
                ValueCheck(
                    name="[1]", summary="size=2", children=expectedStringSpanChildren
                ),
            ],
        )
        self.check_size("nested", 2)

    def do_test_ref_and_ptr(self):
        """Test that std::span is correctly formatted when passed by ref and ptr"""
        (self.target, process, thread, bkpt) = lldbutil.run_to_source_breakpoint(
            self, "Stop here to check by ref", lldb.SBFileSpec("main.cpp", False)
        )

        # The reference should display the same was as the value did
        self.check_numbers("ref")

        # The pointer should just show the right number of elements:

        self.expect("frame variable ptr", patterns=["ptr = 0x[0-9a-f]+ size=5"])

    @skipIf(compiler="clang", compiler_version=["<", "11.0"])
    @add_test_categories(["libc++"])
    def test_libcxx(self):
        self.build(dictionary={"USE_LIBCPP": 1})
        self.do_test()

    @skipIf(compiler="clang", compiler_version=["<", "11.0"])
    @add_test_categories(["libc++"])
    def test_ref_and_ptr_libcxx(self):
        self.build(dictionary={"USE_LIBCPP": 1})
        self.do_test_ref_and_ptr()

    @skipIf(compiler="clang", compiler_version=["<", "11.0"])
    @add_test_categories(["libstdcxx"])
    def test_libstdcxx(self):
        self.build(dictionary={"USE_LIBSTDCPP": 1})
        self.do_test()

    @skipIf(compiler="clang", compiler_version=["<", "11.0"])
    @add_test_categories(["libstdcxx"])
    def test_ref_and_ptr_libstdcxx(self):
        self.build(dictionary={"USE_LIBSTDCPP": 1})
        self.do_test_ref_and_ptr()

    @add_test_categories(["msvcstl"])
    def test_msvcstl(self):
        # No flags, because the "msvcstl" category checks that the MSVC STL is used by default.
        self.build()
        self.do_test()

    @add_test_categories(["msvcstl"])
    def test_ref_and_ptr_msvcstl(self):
        # No flags, because the "msvcstl" category checks that the MSVC STL is used by default.
        self.build()
        self.do_test_ref_and_ptr()
