add_lldb_library(lldbPluginScriptInterpreterSwift PLUGIN
  ScriptInterpreterSwift.cpp
  SwiftInterpreter.cpp

  LINK_LIBS
    lldbCore
    lldbInterpreter
    LLVMOrcJIT
  )
