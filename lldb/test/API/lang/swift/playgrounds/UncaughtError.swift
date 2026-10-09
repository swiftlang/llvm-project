// An error that escapes the top level of a playground reaches
// swift_errorInMain, which ends the process unless LLDB catches it.
enum VagueProblem: Error { case SomethingWentWrong }
func foo() throws -> Int { throw VagueProblem.SomethingWentWrong }
try foo()
