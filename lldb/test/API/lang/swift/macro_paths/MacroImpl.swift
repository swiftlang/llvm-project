import SwiftSyntax
import SwiftSyntaxBuilder
import SwiftSyntaxMacros

public struct StringifyMacro: ExpressionMacro {
  public static func expansion(
    of macro: some FreestandingMacroExpansionSyntax,
    in context: some MacroExpansionContext
  ) -> ExprSyntax {
    let argument = macro.arguments.first!.expression
    return "(\(argument), \(StringLiteralExprSyntax(content: argument.description)))"
  }
}
