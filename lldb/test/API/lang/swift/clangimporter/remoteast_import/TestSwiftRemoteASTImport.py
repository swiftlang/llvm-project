# TestSwiftRemoteASTImport.py
#
# This source file is part of the Swift.org open source project
#
# Copyright (c) 2018 Apple Inc. and the Swift project authors
# Licensed under Apache License v2.0 with Runtime Library Exception
#
# See https://swift.org/LICENSE.txt for license information
# See https://swift.org/CONTRIBUTORS.txt for the list of Swift project authors
#
# ------------------------------------------------------------------------------

import lldb
from lldbsuite.test.lldbtest import *
from lldbsuite.test.decorators import *
import lldbsuite.test.lldbutil as lldbutil
import os

class TestSwiftRemoteASTImport(TestBase):
    @requireNotEmbeddedSwift
    # Don't run ClangImporter tests if Clangimporter is disabled.
    @skipIf(setting=('symbols.use-swift-clangimporter', 'false'))
    @skipIfLinux
    @swiftTest
    def testSwiftRemoteASTImport(self):
        """This tests that RemoteAST querying the dynamic type of a variable
        doesn't import any modules into a module SwiftASTContext that
        weren't imported by that module in the source code.

        FIXME: This does not currently hold. ASTBuilder::findDeclContext()
        calls ASTContext::getModuleByName(), which imports any missing
        module by name, even into a per-module SwiftASTContext. The
        main module's bridging header does not compile in Library's
        context, so once this is fixed, the test should also check that
        no "undeclared identifier 'SYNTAX_ERROR'" error is reported.
        """
        self.build()
        # Validation issues RemoteAST queries a user would not see, and
        # those currently import the main module into Library's context.
        self.runCmd("settings set symbols.swift-validate-typesystem false")

        lldbutil.run_to_source_breakpoint(self, "break here",
                                          lldb.SBFileSpec('Library.swift'),
                                          extra_images=['Library'])
        # FIXME: Reversing the order of these two commands does not work!
        self.expect("expr -d no-dynamic-values -- input",
                    substrs=['(Library.LibraryProtocol) $R0'])
        self.expect("expr -d run-target -- input",
                    substrs=['(a.FromMainModule) $R1', 'i = 1'])
