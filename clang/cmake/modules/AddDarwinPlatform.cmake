# FIXME: How to generalize this to be driven by discovered SDKs?
function(clang_add_additional_platform)
  # Add a platform.
  set(name "xros")
  set(CLANG_PLATFORM_${name}_PLATFORM_AVAILABILITY_NAME "xros" PARENT_SCOPE)
  set(platformFallBack "iphoneos")

  if (NOT "${platformFallBack}" STREQUAL "")
    set(fallbackTripleName "${platformFallBack}")
    # FIXME: we need a generic platform -> triple mapping.
    if ("${fallbackTripleName}" STREQUAL "iphoneos")
      set(fallbackTripleName "ios")
    endif()

    message(STATUS "Platform ${name} has fallback platform - ${fallbackTripleName}")
    set(CLANG_PLATFORM_${name}_FALLBACK_PLATFORM_AVAILABILITY_NAME "${fallbackTripleName}" PARENT_SCOPE)

    # FIXME: This is a hack for xrOS, but should be in the SDKSettings too.
    set(CLANG_PLATFORM_${name}_INFER_UNAVAILABLE 1 PARENT_SCOPE)
    set(CLANG_PLATFORM_${name}_PLATFORM_TRIPLE_OS_VALUE "XROS" PARENT_SCOPE)
    set(CLANG_PLATFORM_${name}_FALLBACK_PLATFORM_TRIPLE_OS_VALUE "IOS" PARENT_SCOPE)
  endif()
endfunction()
