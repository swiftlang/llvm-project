import lldb
from lldbsuite.test.decorators import *
from lldbsuite.test.lldbtest import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftBuiltinBorrow(TestBase):
    def check_var(self, path, **checks):
        """Like expect_var_path, but resolves every component of `path` with
        dynamic types, as `frame variable` does. The Ref formatter applies to
        the resolved type, and SBFrame.GetValueForVariablePath resolves only
        the last component."""
        name, *members = path.split(".")
        value = self.frame().FindVariable(name, lldb.eDynamicCanRunTarget)
        for member in members:
            child = value.GetChildMemberWithName(member)
            if not child.IsValid():
                # Raw members such as `builtin` are hidden behind synthetic
                # children.
                child = value.GetNonSyntheticValue().GetChildMemberWithName(member)
            value = child
        ValueCheck(**checks).check_value(self, value, path)
        return value

    def check_inline(self, path, size, value=None, children=None):
        """Check that the Ref at `path` holds an inline copy of its referent
        and that its synthetic `value` child shows the referent."""
        borrow = self.check_var(
            path + ".builtin", children=[ValueCheck(name="targetValue")]
        )
        self.assertEqual(borrow.GetByteSize(), size)
        self.check_var(path + ".value", value=value, children=children)

    def check_by_pointer(self, path):
        """Check that the Ref at `path` holds a pointer to a Large and that its
        synthetic `value` child shows the pointee."""
        borrow = self.check_var(
            path + ".builtin", children=[ValueCheck(name="targetPointer")]
        )
        self.assertEqual(borrow.GetByteSize(), self.target().GetAddressByteSize())
        value = self.check_var(
            path + ".value",
            children=[
                ValueCheck(name=name, value=str(i)) for i, name in enumerate("abcde", 1)
            ],
        )
        address = borrow.GetChildAtIndex(0).GetValueAsUnsigned()
        self.assertEqual(value.GetLoadAddress(), address)
        return address

    @swiftTest
    def test(self):
        """Test inline and by-pointer Builtin.Borrow values inside Ref."""
        self.build()
        _, process, _, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        word = self.target().GetAddressByteSize()
        small = [
            ValueCheck(name="flag", summary="true"),
            ValueCheck(name="count", value="7"),
        ]

        self.check_inline("holder.byte", 1, value="3")
        self.check_inline("holder.small", 4, children=small)
        self.check_inline("holder.int", word, value="42")
        self.check_by_pointer("holder.large")

        # In a generic context the borrow's representation is only known once
        # the type argument has been resolved.
        def continue_to_generic():
            lldbutil.continue_to_source_breakpoint(
                self, process, "break in generic", lldb.SBFileSpec("main.swift")
            )

        continue_to_generic()
        self.check_inline("ref", 1, value="3")

        continue_to_generic()
        self.check_inline("ref", 4, children=small)

        continue_to_generic()
        address = self.check_by_pointer("ref")
        # A borrowed argument is never copied, so the borrow aliases it.
        value = self.frame().FindVariable("value", lldb.eDynamicCanRunTarget)
        self.assertEqual(address, value.GetLoadAddress())

    @swiftTest
    @expectedFailureAll(bugnumber="rdar://190257759")
    def test_addressable_for_dependencies(self):
        """Test that an addressable-for-dependencies referent is borrowed by
        pointer and that the fields after it are found."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        word = self.target().GetAddressByteSize()

        borrow = self.check_var(
            "stringHolder.string.builtin", children=[ValueCheck(name="targetPointer")]
        )
        self.assertEqual(borrow.GetByteSize(), word)
        self.check_var("stringHolder.string.value", summary='"hello"')

        self.check_inline("stringHolder.int", word, value="42")
        # Another copy of the Int may follow the holder in memory, so check the
        # layout directly rather than relying on the value alone.
        holder = self.frame().FindVariable("stringHolder")
        self.assertEqual(holder.GetByteSize(), 2 * word)
        int_field = holder.GetChildMemberWithName("int")
        self.assertEqual(int_field.GetLoadAddress() - holder.GetLoadAddress(), word)

    def raw_bytes(self, value, size):
        error = lldb.SBError()
        data = value.GetData().ReadRawData(error, 0, size)
        self.assertSuccess(error)
        return data

    @swiftTest
    @expectedFailureAll(bugnumber="rdar://190256189")
    def test_layout_rule_sensitive_referent(self):
        """Test a borrow whose representation depends on whether it's chosen by
        size or by calling convention, in code that uses the compiler's static
        layout."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = self.frame()
        floats = frame.FindVariable("floats")
        size = floats.GetByteSize()
        # `value` follows whichever representation LLDB decodes the borrow
        # with, so its bytes are the referent's only if LLDB picked the one the
        # compiler used.
        self.assertEqual(
            self.raw_bytes(self.check_var("mismatchHolder.floats.value"), size),
            self.raw_bytes(floats, size),
        )
        self.assertEqual(
            frame.FindVariable("mismatchHolder").GetByteSize(),
            frame.FindVariable("mismatchHolderSize").GetValueAsSigned(),
        )
        self.check_var("mismatchHolder.int.value", value="42")

    @swiftTest
    @requireNotEmbeddedSwift
    @expectedFailureAll(bugnumber="rdar://190256189")
    def test_layout_rules_agree(self):
        """Test that the compiler's static layout and the runtime's type
        metadata agree on the size of a Ref whose borrow representation depends
        on the layout rule."""
        self.build()
        lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = self.frame()
        for name in ["floats", "vectors"]:
            self.assertEqual(
                frame.FindVariable(name + "StaticRefSize").GetValueAsSigned(),
                frame.FindVariable(name + "RuntimeRefSize").GetValueAsSigned(),
                name,
            )
