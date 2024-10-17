cmake_minimum_required(VERSION 3.22)

project(SwiftHighlighter LANGUAGES Swift)

# Verify that we have a new enough compiler
if("${CMAKE_Swift_COMPILER_VERSION}" VERSION_LESS 5.9)
  message(FATAL_ERROR "Bidirectional C++ Interop requires Swift 5.9 or greater. Have ${CMAKE_Swift_COMPILER_VERSION}")
endif()

if(NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" AND
   NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "AppleClang")
  message(FATAL_ERROR "Project requires building with Clang.
  Have ${CMAKE_CXX_COMPILER_ID}")
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modules")

# Set up swiftrt.o and runtime library search paths
include(InitializeSwift)
# Include the _swift_generate_cxx_header function
include(GenerateCxxHeader)


add_library(swiftHighlighter STATIC swiftHighlighter.swift)
target_link_libraries(swiftHighlighter SwiftSyntax)
target_link_libraries(swiftHighlighter SwiftParser)

target_compile_options(swiftHighlighter PUBLIC
  "$<$<COMPILE_LANGUAGE:Swift>:-cxx-interoperability-mode=default>")


set(SWIFT_HIGHLIGHTER_INCLUDE_DIR "${LLDB_SWIFT_LIBS}/host/"
  "${CMAKE_CURRENT_SOURCE_DIR}/include")

_swift_generate_cxx_header(swiftHighlighter
  swiftHighlighter/swiftHighlighter-swift.h
  SEARCH_PATHS "${SWIFT_HIGHLIGHTER_INCLUDE_DIR}")
