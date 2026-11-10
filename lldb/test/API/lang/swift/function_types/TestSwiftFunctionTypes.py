import lldb
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbtest as lldbtest
import lldbsuite.test.lldbutil as lldbutil


class TestCase(lldbtest.TestBase):

    def function(self, base_name, name):
        """Return the function named base_name whose full name contains name."""
        for context in self.target().FindFunctions(base_name):
            function = context.GetFunction()
            if name in function.GetName():
                return function
        self.fail("no function " + name)

    def check_type(self, function, type_name, return_type_name):
        function_type = function.GetType()
        self.assertTrue(function_type.IsFunctionType(), function.GetName())
        self.assertEqual(function_type.GetName(), type_name)
        self.assertEqual(
            function_type.GetFunctionReturnType().GetName(), return_type_name
        )

    @swiftTest
    # FIXME: Only DWARFASTParserSwift gives a function the type its linkage
    # name mangles.
    @expectedFailureAll(debug_info=["pdb"])
    def test(self):
        """Test that Swift functions have the type their linkage name mangles"""
        self.build()
        _, process, thread, _ = lldbutil.run_to_source_breakpoint(
            self, "break in getter", lldb.SBFileSpec("main.swift")
        )
        self.check_type(thread.frames[0].GetFunction(), "() -> Swift.Int", "Swift.Int")
        thread = lldbutil.continue_to_source_breakpoint(
            self, process, "break in closure", lldb.SBFileSpec("main.swift")
        )[0]
        self.check_type(
            thread.frames[0].GetFunction(), "(Swift.Int) -> Swift.Int", "Swift.Int"
        )
        self.check_type(
            self.function("noArguments", "a.noArguments("),
            "() -> Swift.Int",
            "Swift.Int",
        )
        self.check_type(
            self.function("twoArguments", "a.twoArguments("),
            "(Swift.Int, Swift.String) -> Swift.Bool",
            "Swift.Bool",
        )
        self.check_type(
            self.function("throwing", "a.throwing("),
            "(Swift.Int) throws -> Swift.Int",
            "Swift.Int",
        )
        arguments = (
            self.function("throwing", "a.throwing(")
            .GetType()
            .GetFunctionArgumentTypes()
        )
        self.assertEqual(arguments.GetSize(), 1)
        self.assertEqual(arguments.GetTypeAtIndex(0).GetName(), "Swift.Int")
        self.check_type(
            self.function("method", "a.C.method("),
            "(Swift.Int) -> Swift.String",
            "Swift.String",
        )
        self.check_type(
            self.function("staticMethod", "a.C.staticMethod("),
            "() -> Swift.Int",
            "Swift.Int",
        )
        self.check_type(
            self.function("init", "a.C.init(x:"), "(Swift.Int) -> a.C", "a.C"
        )
        self.check_type(
            self.function("mutatingMethod", "a.S.mutatingMethod("),
            "() -> Swift.Int",
            "Swift.Int",
        )
        self.check_type(
            self.function("returnsClosure", "a.returnsClosure("),
            "() -> (Swift.Int) -> Swift.Int",
            "(Swift.Int) -> Swift.Int",
        )
