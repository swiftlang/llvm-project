@_spi(PluginMessage) import SwiftCompilerPluginMessageHandling
import SwiftSyntaxMacros

struct Provider: PluginProvider {
  func resolveMacro(moduleName: String, typeName: String) throws -> Macro.Type {
    precondition(moduleName == "MacroImpl" && typeName == "StringifyMacro")
    return StringifyMacro.self
  }
}

@main
struct Plugin {
  static func main() throws {
    let connection = try StandardIOMessageConnection()
    let listener = CompilerPluginMessageListener(connection: connection, provider: Provider())
    try listener.main()
  }
}
