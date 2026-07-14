# lldbExpression no longer references the Swift expression parser plugin (only
# a comment in UserExpression.cpp mentions SwiftASTContext). liblldb still links
# lldbPluginExpressionParserSwift via LLDB_ALL_PLUGINS; keeping it out of core
# lldbExpression stops SwiftASTContext / the Swift compiler from being
# force-linked into lldb-server, which links lldbExpression but not that plugin.
set(SWIFT_LIBS)
set(PLUGIN_DEPENDENCY_ARG)
if(NOT LLDB_ENABLE_SWIFT_SUPPORT)
  unset(SWIFT_LIBS)
  set(PLUGIN_DEPENDENCY_ARG NO_PLUGIN_DEPENDENCIES)
endif()

add_lldb_library(lldbExpression ${PLUGIN_DEPENDENCY_ARG}
  DiagnosticManager.cpp
  DWARFExpression.cpp
  DWARFExpressionList.cpp
  Expression.cpp
  ExpressionParser.cpp
  ExpressionTypeSystemHelper.cpp
  ExpressionVariable.cpp
  FunctionCaller.cpp
  IRExecutionUnit.cpp
  IRInterpreter.cpp
  IRMemoryMap.cpp
  LLVMUserExpression.cpp
  Materializer.cpp
  ObjectFileJIT.cpp
  REPL.cpp
  UserExpression.cpp
  UtilityFunction.cpp

  ADDITIONAL_HEADER_DIRS
    ${LLDB_INCLUDE_DIR}/lldb/Expression
  DEPENDS
  intrinsics_gen

  LINK_COMPONENTS
    Core
    ExecutionEngine
    Support
  LINK_LIBS
    lldbCore
    lldbHost
    lldbInterpreter
    lldbSymbol
    lldbTarget
    lldbUtility
    lldbValueObject

    ## BEGIN SWIFT
    ${SWIFT_LIBS}
    ## END SWIFT

  LINK_COMPONENTS
    Core
    ExecutionEngine
    Support
  )
