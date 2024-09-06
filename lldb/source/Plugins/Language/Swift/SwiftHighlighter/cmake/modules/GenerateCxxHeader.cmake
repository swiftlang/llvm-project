# Generate the bridging header from Swift to C++
#
# target: the name of the target to generate headers for.
#         This target must build swift source files.
# header: the name of the header file to generate.
#
# NOTE: This logic will eventually be unstreamed into CMake.
function(_swift_generate_cxx_header target header)
  if(NOT TARGET ${target})
    message(FATAL_ERROR "Target ${target} not defined.")
  endif()

  if(NOT DEFINED CMAKE_Swift_COMPILER)
    message(WARNING "Swift not enabled in project. Cannot generate headers for Swift files.")
    return()
  endif()

  cmake_parse_arguments(ARG "" "" "SEARCH_PATHS;MODULE_NAME" ${ARGN})

  if(NOT ARG_MODULE_NAME)
    set(target_module_name $<TARGET_PROPERTY:${target},Swift_MODULE_NAME>)
    set(ARG_MODULE_NAME $<IF:$<BOOL:${target_module_name}>,${target_module_name},${target}>)
  endif()

  if(ARG_SEARCH_PATHS)
    list(TRANSFORM ARG_SEARCH_PATHS PREPEND "-I")
  endif()

  if(APPLE)
    set(SDK_FLAGS "-sdk" "${CMAKE_OSX_SYSROOT}")
  elseif(WIN32)
    set(SDK_FLAGS "-sdk" "$ENV{SDKROOT}")
  elseif(DEFINED ${CMAKE_SYSROOT})
    set(SDK_FLAGS "-sdk" "${CMAKE_SYSROOT}")
  endif()

  cmake_path(APPEND CMAKE_CURRENT_BINARY_DIR include
    OUTPUT_VARIABLE base_path)

  cmake_path(APPEND base_path ${header}
    OUTPUT_VARIABLE header_path)

  set(_AllSources $<TARGET_PROPERTY:${target},SOURCES>)
  set(_SwiftSources $<FILTER:${_AllSources},INCLUDE,\\.swift$>)
  add_custom_command(OUTPUT ${header_path}
    DEPENDS ${_SwiftSources}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND
      ${CMAKE_Swift_COMPILER} -frontend -typecheck
      ${ARG_SEARCH_PATHS}
      ${_SwiftSources}
      ${SDK_FLAGS}
      -module-name "${ARG_MODULE_NAME}"
      -cxx-interoperability-mode=default
      -emit-clang-header-path ${header_path}
      -Xcc -std=c++17
    COMMENT
      "Generating '${header_path}'"
    COMMAND_EXPAND_LISTS)

  # Added to public interface for dependees to find.
  target_include_directories(${target} PUBLIC ${base_path})
  # Added to the target to ensure target rebuilds if header changes and is used
  # by sources in the target.
  target_sources(${target} PRIVATE ${header_path})
endfunction()
