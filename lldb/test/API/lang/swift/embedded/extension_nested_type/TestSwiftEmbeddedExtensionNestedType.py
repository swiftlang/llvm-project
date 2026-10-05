import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil


class TestSwiftEmbeddedExtensionNestedType(TestBase):
    @skipEmbeddedSwiftOnWindows
    @skipUnlessEmbeddedSwift
    @swiftTest
    def test(self):
        """Test that a type declared in an extension whose mangled decl context
        contains an Extension node can be laid out from DWARF."""
        self.build()
        self.runCmd("setting set symbols.swift-enable-ast-context false")
        _, _, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break here", lldb.SBFileSpec("main.swift")
        )
        frame = thread.frames[0]
        self.assertTrue(frame, "Frame 0 is valid.")

        # Control: an unconstrained same-module extension is elided by the
        # mangler, so this has always worked.
        plain = frame.FindVariable("plain")
        lldbutil.check_variable(self, plain, num_children=1)
        lldbutil.check_variable(
            self, plain.GetChildMemberWithName("v"), value="10"
        )

        # A constrained extension of a generic struct.
        constr = frame.FindVariable("constr")
        lldbutil.check_variable(self, constr, num_children=1)
        lldbutil.check_variable(
            self, constr.GetChildMemberWithName("v"), value="20"
        )

        # A private type in a constrained extension. Reaching `hidden` requires
        # resolving a decl context that has a private discriminator underneath
        # the Extension node.
        holder = frame.FindVariable("holder")
        lldbutil.check_variable(self, holder, num_children=2)
        lldbutil.check_variable(
            self,
            holder.GetChildMemberWithName("hidden").GetChildMemberWithName("w"),
            value="30",
        )
        lldbutil.check_variable(
            self, holder.GetChildMemberWithName("tag"), value="31"
        )

        # A constrained extension of a generic class.
        cls = frame.FindVariable("cls")
        lldbutil.check_variable(self, cls, num_children=1)
        lldbutil.check_variable(self, cls.GetChildMemberWithName("v"), value="40")
