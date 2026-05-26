set(LLVM_LINK_COMPONENTS
  Core
  Demangle
  FrontendHLSL
  FrontendOpenMP
  MC
  Support
  TargetParser
  )

clang_tablegen(OpenCLBuiltins.inc -gen-clang-opencl-builtins
  SOURCE OpenCLBuiltins.td
  TARGET ClangOpenCLBuiltinsImpl
  )

# Configure additional platform-specific code that controls things like
# availability attributes.
set(TEMPLATE_INCLUDES_INFER_AVAILABILITY_ATTRIBUTE "")

foreach(supported_platform ${LLVM_SUPPORTED_PLATFORMS})
  if (${CLANG_PLATFORM_${supported_platform}_INFER_UNAVAILABLE})
    # Generate code that infers availability(unavailable) attribute if a platform
    # requires that.
    set(output_file_name "InferUnavailableAvailabilityFromFallbackPlatform_${supported_platform}.def")
    message(STATUS "Adding platform specific availability attribute code - ${output_file_name}")

    set(PLATFORM_TRIPLE_OS_VALUE
        "${CLANG_PLATFORM_${supported_platform}_PLATFORM_TRIPLE_OS_VALUE}")
    set(FALLBACK_PLATFORM_TRIPLE_OS_VALUE
        "${CLANG_PLATFORM_${supported_platform}_FALLBACK_PLATFORM_TRIPLE_OS_VALUE}")
    set(PLATFORM_AVAILABILITY_NAME
        "${CLANG_PLATFORM_${supported_platform}_PLATFORM_AVAILABILITY_NAME}")
    set(FALLBACK_PLATFORM_AVAILABILITY_NAME
        "${CLANG_PLATFORM_${supported_platform}_FALLBACK_PLATFORM_AVAILABILITY_NAME}")
    configure_file(
      ${CMAKE_CURRENT_SOURCE_DIR}/PlatformTemplates/InferUnavailableAvailabilityFromFallbackPlatform.def.in
      ${CMAKE_CURRENT_BINARY_DIR}/PlatformTemplates/${output_file_name}
    )
    set(TEMPLATE_INCLUDES_INFER_AVAILABILITY_ATTRIBUTE "${TEMPLATE_INCLUDES_INFER_AVAILABILITY_ATTRIBUTE}\n#include \"PlatformTemplates/${output_file_name}\"")
  endif()
endforeach()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/InferAvailabilityAttribute.cpp.in
  ${CMAKE_CURRENT_BINARY_DIR}/InferAvailabilityAttribute.cpp
)

add_clang_library(clangSema
  AnalysisBasedWarnings.cpp
  # TO_UPSTREAM(BoundsSafety) ON
  BoundsSafetySuggestions.cpp
  # TO_UPSTREAM(BoundsSafety) OFF
  CheckExprLifetime.cpp
  CodeCompleteConsumer.cpp
  DeclSpec.cpp
  DelayedDiagnostic.cpp
  # TO_UPSTREAM(BoundsSafety) ON
  DynamicCountPointerAssignmentAnalysis.cpp
  DynamicCountPointerAssignmentAnalysisExported.cpp
  # TO_UPSTREAM(BoundsSafety) OFF
  HeuristicResolver.cpp
  HLSLBuiltinTypeDeclBuilder.cpp
  HLSLExternalSemaSource.cpp
  IdentifierResolver.cpp
  JumpDiagnostics.cpp
  MultiplexExternalSemaSource.cpp
  ParsedAttr.cpp
  Scope.cpp
  ScopeInfo.cpp
  Sema.cpp
  SemaAMDGPU.cpp
  SemaARM.cpp
  SemaAVR.cpp
  SemaAccess.cpp
  SemaAttr.cpp
  SemaAPINotes.cpp
  SemaAvailability.cpp
  SemaBPF.cpp
  SemaBase.cpp
  SemaBoundsSafety.cpp
  SemaCXXScopeSpec.cpp
  SemaCast.cpp
  SemaChecking.cpp
  SemaCodeComplete.cpp
  SemaConcept.cpp
  SemaConsumer.cpp
  SemaCoroutine.cpp
  SemaCUDA.cpp
  SemaDirectX.cpp
  SemaDecl.cpp
  SemaDeclAttr.cpp
  SemaDeclCXX.cpp
  SemaDeclObjC.cpp
  SemaExceptionSpec.cpp
  SemaExpr.cpp
  SemaExprCXX.cpp
  SemaExprMember.cpp
  SemaExprObjC.cpp
  SemaFeatureAvailability.cpp
  SemaFixItUtils.cpp
  SemaFunctionEffects.cpp
  SemaHLSL.cpp
  SemaHexagon.cpp
  SemaInit.cpp
  SemaLambda.cpp
  SemaLookup.cpp
  SemaLoongArch.cpp
  SemaM68k.cpp
  SemaMIPS.cpp
  SemaMSP430.cpp
  SemaModule.cpp
  SemaNVPTX.cpp
  SemaObjC.cpp
  SemaObjCProperty.cpp
  SemaOpenACC.cpp
  SemaOpenACCAtomic.cpp
  SemaOpenACCClause.cpp
  SemaOpenACCClauseAppertainment.cpp
  SemaOpenCL.cpp
  SemaOpenMP.cpp
  SemaOverload.cpp
  SemaPPC.cpp
  SemaPseudoObject.cpp
  SemaRISCV.cpp
  SemaStmt.cpp
  SemaStmtAsm.cpp
  SemaStmtAttr.cpp
  SemaSPIRV.cpp
  SemaSYCL.cpp
  SemaSwift.cpp
  SemaSystemZ.cpp
  SemaTemplate.cpp
  SemaTemplateDeduction.cpp
  SemaTemplateDeductionGuide.cpp
  SemaTemplateInstantiate.cpp
  SemaTemplateInstantiateDecl.cpp
  SemaTemplateVariadic.cpp
  SemaType.cpp
  SemaTypeTraits.cpp
  SemaWasm.cpp
  SemaX86.cpp
  TypeLocBuilder.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/InferAvailabilityAttribute.cpp

  DEPENDS
  ClangOpenCLBuiltinsImpl
  acc_gen
  omp_gen
  ClangDriverOptions

  LINK_LIBS
  clangAPINotes
  clangAST
  clangAnalysis
  clangBasic
  clangEdit
  clangLex
  clangSupport
  )
