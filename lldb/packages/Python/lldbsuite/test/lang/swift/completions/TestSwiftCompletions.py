# TestSwiftCompletions.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2019 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------

import lldb
from lldbsuite.test.lldbtest import *
import unittest2


class TestSwiftCompletions(TestBase):

    mydir = TestBase.compute_mydir(__file__)

    @decorators.swiftTest
    def test_completions(self):
        self.build()
        self.do_test()

    def evaluate(self, code):
        # TODO(TF-107): Remove this workaround.
        code = '#sourceLocation(file: "test", line: 1)\n' + code

        result = self.target.EvaluateExpression(code, self.expr_opts)
        if result.error.type not in [lldb.eErrorTypeInvalid,
                                     lldb.eErrorTypeGeneric]:
          raise Exception("while evaluating:\n%s\n\nerror: %s" % (
              code, str(result.error)))

    def assertCompletions(self, code, expected_completions, check_order=False):
        sbcompletions = self.target.CompleteCode(self.swift_lang, None, code)
        completions = []
        for i in range(sbcompletions.GetNumMatches()):
            completions.append(
                sbcompletions.GetMatchAtIndex(i).GetInsertable())
        if not check_order:
            completions.sort()
            expected_completions.sort()
        self.assertTrue(completions == expected_completions,
                        "expected: %s, got: %s" % (str(expected_completions),
                                                   str(completions)))

    def assertDisplayCompletions(self, code, expected_completions,
                                 check_order=False):
        sbcompletions = self.target.CompleteCode(self.swift_lang, None, code)
        completions = []
        for i in range(sbcompletions.GetNumMatches()):
            completions.append(
                sbcompletions.GetMatchAtIndex(i).GetDisplay())
        if not check_order:
            completions.sort()
            expected_completions.sort()
        self.assertTrue(completions == expected_completions,
                        "expected: %s, got: %s" % (str(expected_completions),
                                                   str(completions)))

    def do_test(self):
        exe_name = "a.out"
        exe = self.getBuildArtifact(exe_name)

        self.expr_opts = lldb.SBExpressionOptions()
        self.swift_lang = lldb.SBLanguageRuntime.GetLanguageTypeFromString(
            "swift")
        self.expr_opts.SetLanguage(self.swift_lang)

        # REPL mode is necessary for previously-evaluated decls to persist.
        self.expr_opts.SetREPLMode(True)

        self.main_source = "main.swift"
        self.main_source_spec = lldb.SBFileSpec(self.main_source)
        self.target = self.dbg.CreateTarget(exe)
        self.assertTrue(self.target, VALID_TARGET)
        self.breakpoint = self.target.BreakpointCreateBySourceRegex(
            "Set breakpoint here", self.main_source_spec)
        self.assertTrue(self.breakpoint.GetNumLocations() > 0, VALID_BREAKPOINT)
        self.process = self.target.LaunchSimple(None, None, os.getcwd())
        self.assertTrue(self.process, PROCESS_IS_VALID)

        # === Simple completions involving a custom struct ===

        self.evaluate("""
                      struct SimpleStruct {
                        let intfield: Int = 10
                        let strfield: String = "hello world"
                      }""")
        self.assertCompletions(
            """let s = SimpleSt""",
            ["ruct"])

        self.evaluate("""let simpleValue = SimpleStruct()""")
        self.assertCompletions(
            """simpleVa""",
            ["lue"])
        self.assertCompletions(
            """simpleValue.""",
            ["intfield", "strfield", "self"])

        # === Redefining a value ===

        self.evaluate("""
                      struct DifferentStruct {
                        let differentfield: Int = 1
                      }""")
        self.evaluate("""let replacedValue = SimpleStruct()""")
        # TODO: If you put the quotes that are after "replacedValue." on the
        # next line, then this completion request segfaults. Perhaps newlines
        # and/or whitespace confuse the completer? Investigate.
        self.assertCompletions(
            """
            let replacedValue = DifferentStruct()
            replacedValue.""",
            ["differentfield", "self"])
        self.evaluate("""let replacedValue = DifferentStruct()""")
        self.assertCompletions(
            """replacedValue.""",
            ["differentfield", "self"])

        # === Redefining a type ===

        self.evaluate("""
                      struct ReplacedStruct {
                        let field1: Int = 1
                      }
                      """)
        self.assertCompletions(
            """
            struct ReplacedStruct {
              let field2: Int = 2
            }
            ReplacedStruct().""",
            ["field2", "self"])
        self.evaluate("""
                      struct ReplacedStruct {
                        let field2: Int = 2
                      }
                      """)
        self.assertCompletions(
            """ReplacedStruct().""",
            ["field2", "self"])

        # === Redefining a func ===

        self.evaluate("""
                      func replacedFunc(arglabel: Int) {}
                      """)
        self.assertDisplayCompletions(
            """
            func replacedFunc(arglabel: Int) -> Int { return 42 }
            replacedFun""",
            ["replacedFunc(arglabel: Int) -> Int"])
        self.assertDisplayCompletions(
            """
            func replacedFunc(differentArglabel: Int) {}
            replacedFun""",
            ["replacedFunc(arglabel: Int) -> Void",
             "replacedFunc(differentArglabel: Int) -> Void"])
        self.evaluate("""
                      func replacedFunc(arglabel: Int) -> Int { return 42 }
                      """)
        # Huh? Both versions of the func show up in the completion below. Let's
        # keep the test to document the current behavior, and we can figure out
        # later why it happens.
        self.assertDisplayCompletions(
            """replacedFun""",
            ["replacedFunc(arglabel: Int) -> Void",
             "replacedFunc(arglabel: Int) -> Int"])
        self.evaluate("""
                      func replacedFunc(differentArglabel: Int) {}
                      """)
        self.assertDisplayCompletions(
            """replacedFun""",
            ["replacedFunc(arglabel: Int) -> Void",
             "replacedFunc(arglabel: Int) -> Int",
             "replacedFunc(differentArglabel: Int) -> Void"])

        # === Redefining a func in an extension ===

        # This has bad behavior, explained inline in comments below. These tests
        # document the behavior and we will investigate later.

        self.evaluate("""struct Base {}""")
        self.evaluate("""
                      extension Base {
                        func replacedFunc() {}
                      }
                      """)
        self.assertCompletions(
            """
            extension Base {
              func replacedFunc () {}
            }
            Base().""",
            ["replacedFunc()", "self"])
        # Asking for completions multiple times when an extension is defined in
        # your cell causes the extenion's function to appear multiple times in
        # the completion.
        self.assertCompletions(
            """
            extension Base {
              func replacedFunc () {}
            }
            Base().""",
            ["replacedFunc", "replacedFunc()", "self"])
        self.evaluate("""
                      extension Base {
                        func replacedFunc() {}
                      }
                      """)
        self.assertCompletions(
            """Base().""",
            ["replacedFunc", "replacedFunc()", "self"])

        # === Redefining a func in an extension ===

        # This has bad behavior, explained inline in comments below. These tests
        # document the behavior and we will investigate later.

        self.evaluate("""struct Base {}""")
        self.evaluate("""
                      extension Base {
                        struct ReplacedStruct {}
                      }
                      """)
        # Asking for a completion in a cell that redefines an extension type
        # gives you an extra copy of the type.
        self.assertCompletions(
            """
            extension Base {
              struct ReplacedStruct {}
            }
            Base.""",
            ["ReplacedStruct", "ReplacedStruct", "Type", "init()", "self"])
        # Asking for a completion again adds another copy of the type.
        self.assertCompletions(
            """
            extension Base {
              struct ReplacedStruct {}
            }
            Base.""",
            ["ReplacedStruct", "ReplacedStruct", "ReplacedStruct", "Type",
             "init()", "self"])
        # Executing the extension adds another copy of the type.
        self.evaluate("""
                      extension Base {
                        struct ReplacedStruct {}
                      }
                      """)
        self.assertCompletions(
            """Base.""",
            ["ReplacedStruct", "ReplacedStruct", "ReplacedStruct",
             "ReplacedStruct", "Type", "init()", "self"])

        # === TF-249: Crashes when completing a keypath naming a struct with a
        #             private field ===

        self.assertCompletions(
            """
            struct TF249StructWithPrivateField {
              private let x: Float
              func bla() {
                \TF249Struct""",
            ["WithPrivateField"])

        # TODO: Test imports.
