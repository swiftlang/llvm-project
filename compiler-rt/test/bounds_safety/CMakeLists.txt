message(STATUS "Trying to enable -fbounds-safety runtime tests")
if (NOT APPLE)
  # FIXME: We should support Linux too.
  message(STATUS "Skipping -fbounds-safety runtime tests on non-Apple platforms")
  return()
endif()
set(BOUNDS_SAFETY_LIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BOUNDS_SAFETY_HOST_TEST_SUITES)
# Currently no dependencies
set(BOUNDS_SAFETY_TEST_DEPS "")

# Detect LLDB availability and Python compatibility
find_program(LLDB_EXECUTABLE lldb HINTS ${LLVM_TOOLS_DIR})
set(_LLDB_DETECTED FALSE)
set(_LLDB_PYTHON_COMPATIBLE FALSE)
if(LLDB_EXECUTABLE)
  execute_process(
    COMMAND ${LLDB_EXECUTABLE} -P
    OUTPUT_VARIABLE BOUNDS_SAFETY_LLDB_PYTHON_PATH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE LLDB_P_RESULT
  )
  if(LLDB_P_RESULT EQUAL 0)
    set(_LLDB_DETECTED TRUE)
    # Check that the Python interpreter used by lit can import the lldb module.
    # A mismatch (e.g. Homebrew Python 3.13 vs Xcode LLDB built for Python 3.11)
    # causes a confusing ImportError at test time.
    execute_process(
      COMMAND ${Python3_EXECUTABLE} -c
        "import sys; sys.path.insert(0, '${BOUNDS_SAFETY_LLDB_PYTHON_PATH}'); import lldb"
      RESULT_VARIABLE _LLDB_IMPORT_RESULT
      ERROR_VARIABLE _LLDB_IMPORT_ERROR
      OUTPUT_QUIET
    )
    if(_LLDB_IMPORT_RESULT EQUAL 0)
      set(_LLDB_PYTHON_COMPATIBLE TRUE)
    endif()
  endif()
endif()

# CMake option: defaults to ON if LLDB detected AND the Python interpreter
# can import the lldb module. If explicitly set to ON but either condition
# fails, error the build.
option(COMPILER_RT_BOUNDS_SAFETY_USE_LLDB
  "Use LLDB for bounds-safety debugger test suites" ${_LLDB_PYTHON_COMPATIBLE})

if(COMPILER_RT_BOUNDS_SAFETY_USE_LLDB AND NOT _LLDB_DETECTED)
  message(FATAL_ERROR
    "COMPILER_RT_BOUNDS_SAFETY_USE_LLDB=ON but LLDB was not found or "
    "lldb -P failed. Set to OFF or ensure LLDB is available.")
endif()

if(COMPILER_RT_BOUNDS_SAFETY_USE_LLDB AND NOT _LLDB_PYTHON_COMPATIBLE)
  message(FATAL_ERROR
    "COMPILER_RT_BOUNDS_SAFETY_USE_LLDB=ON but the Python interpreter "
    "used by lit (${Python3_EXECUTABLE}) cannot import the lldb module "
    "from ${BOUNDS_SAFETY_LLDB_PYTHON_PATH}.\n"
    "This typically means Python and LLDB were built for different Python "
    "versions. Either:\n"
    "  - Set Python3_EXECUTABLE to the Python that matches LLDB "
    "(e.g. the one bundled with Xcode), or\n"
    "  - Set COMPILER_RT_BOUNDS_SAFETY_USE_LLDB=OFF to disable debugger "
    "test suites.\n"
    "Import error: ${_LLDB_IMPORT_ERROR}")
endif()

if (COMPILER_RT_BOUNDS_SAFETY_USE_LLDB)
  message(STATUS "Enabled -fbounds-safety tests using LLDB: ${BOUNDS_SAFETY_LLDB_PYTHON_PATH}")
else()
  message(STATUS "Disabled -fbounds-safety tests using LLDB")
endif()

set(BOUNDS_SAFETY_OPT_LEVELS unopt opt)
set(BOUNDS_SAFETY_TRAP_KINDS unique-traps merged-traps soft-traps)
set(BOUNDS_SAFETY_RUN_MODES direct)
if(COMPILER_RT_BOUNDS_SAFETY_USE_LLDB)
  list(APPEND BOUNDS_SAFETY_RUN_MODES debugger)
endif()


list(FIND SANITIZER_COMMON_SUPPORTED_OS "osx" OSX_INDEX)
if (${OSX_INDEX} EQUAL -1)
  message(FATAL_ERROR "Support for osx is missing")
endif()

# TODO: Add support for other platforms
set(BOUNDS_SAFETY_APPLE_PLATFORMS osx)

foreach(platform ${BOUNDS_SAFETY_APPLE_PLATFORMS})
  # Determine archs for this platform
  if("${platform}" STREQUAL "osx")
    # For osx, filter to host-compatible archs
    set(PLATFORM_ARCHS ${DARWIN_osx_ARCHS})
    darwin_filter_host_archs(PLATFORM_ARCHS PLATFORM_ARCHS)
    set(BOUNDS_SAFETY_TEST_APPLE_TARGET_IS_HOST ON)
  else()
    # For other platforms, use all available archs
    set(PLATFORM_ARCHS ${DARWIN_${platform}_ARCHS})
    set(BOUNDS_SAFETY_TEST_APPLE_TARGET_IS_HOST OFF)
  endif()
  pythonize_bool(BOUNDS_SAFETY_TEST_APPLE_TARGET_IS_HOST)

  set(BOUNDS_SAFETY_TEST_APPLE_PLATFORM "${platform}")
  set(BOUNDS_SAFETY_TEST_MIN_DEPLOYMENT_TARGET_FLAG
      "${DARWIN_${platform}_MIN_VER_FLAG}")

  foreach(arch ${PLATFORM_ARCHS})
    get_test_cflags_for_apple_platform(
      "${platform}" "${arch}" BOUNDS_SAFETY_TEST_TARGET_CFLAGS)

    foreach(opt_level ${BOUNDS_SAFETY_OPT_LEVELS})
      # Determine which trap kinds apply for this opt level.
      # At -O0 merged traps are identical to unique traps, so skip.
      if("${opt_level}" STREQUAL "unopt")
        set(ACTIVE_TRAP_KINDS unique-traps soft-traps)
      else()
        set(ACTIVE_TRAP_KINDS ${BOUNDS_SAFETY_TRAP_KINDS})
      endif()

      foreach(trap_kind ${ACTIVE_TRAP_KINDS})
        set(BOUNDS_SAFETY_TEST_TRAP_KIND "${trap_kind}")

        foreach(mode ${BOUNDS_SAFETY_RUN_MODES})
          set(BOUNDS_SAFETY_TEST_TARGET_ARCH ${arch})

          if("${platform}" STREQUAL "osx")
            set(CONFIG_NAME "host")
          else()
            set(CONFIG_NAME "remote")
          endif()
          string(APPEND CONFIG_NAME "-${arch}-apple-${platform}--${opt_level}--${trap_kind}--${mode}")

          if("${opt_level}" STREQUAL "unopt")
            set(BOUNDS_SAFETY_TEST_OPTIMIZED FALSE)
          else()
            set(BOUNDS_SAFETY_TEST_OPTIMIZED TRUE)
          endif()
          pythonize_bool(BOUNDS_SAFETY_TEST_OPTIMIZED)

          if("${mode}" STREQUAL "debugger")
            set(BOUNDS_SAFETY_TEST_USE_DEBUGGER TRUE)
          else()
            set(BOUNDS_SAFETY_TEST_USE_DEBUGGER FALSE)
          endif()
          pythonize_bool(BOUNDS_SAFETY_TEST_USE_DEBUGGER)

          configure_compiler_rt_lit_site_cfg(
            ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
            ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME}/lit.site.cfg.py
          )

          if("${platform}" STREQUAL "osx")
            # Host tests: accumulate into main check-bounds-safety target
            list(APPEND BOUNDS_SAFETY_HOST_TEST_SUITES
                 ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME})
          else()
            # Non-host: individual targets, excluded from check-all
            add_lit_testsuite(
              check-bounds-safety-remote-${platform}-${arch}-${opt_level}-${trap_kind}-${mode}
              "bounds-safety remote ${platform} ${arch} ${opt_level} ${trap_kind} ${mode} tests"
              ${CMAKE_CURRENT_BINARY_DIR}/${CONFIG_NAME}
              EXCLUDE_FROM_CHECK_ALL
              DEPENDS ${BOUNDS_SAFETY_TEST_DEPS})
          endif()
        endforeach()
      endforeach()
    endforeach()
  endforeach()
endforeach()

add_lit_testsuite(check-bounds-safety
  "Running the bounds-safety runtime tests"
  ${BOUNDS_SAFETY_HOST_TEST_SUITES}
  DEPENDS ${BOUNDS_SAFETY_TEST_DEPS})
