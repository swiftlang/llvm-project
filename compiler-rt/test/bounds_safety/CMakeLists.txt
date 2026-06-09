message(STATUS "Trying to enable -fbounds-safety runtime tests")

# Tri-state control over which LLDB the bounds-safety debugger test suites use.
# Defined before the early `return()` below so it is always available as a cache
# variable in both the standalone build and the LLVM_ENABLE_RUNTIMES build.
#   ON   - only the just-built (in-tree) LLDB is considered; warn if unusable.
#   OFF  - only a system LLDB is considered; the just-built LLDB is excluded.
#   AUTO - consider both, preferring the just-built LLDB and falling back to the
#          system one if the just-built LLDB is not usable.
set(COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB "AUTO" CACHE STRING
  "Which LLDB the bounds-safety debugger tests use: \
ON (just-built only, warns if unusable), OFF (system only), \
AUTO (prefer just-built, fall back to system)")
set_property(CACHE COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB
  PROPERTY STRINGS ON OFF AUTO)
# Canonicalize to upper case so the comparisons below are case-insensitive
# (e.g. "auto", "On", "off" are all accepted), and reject anything else.
string(TOUPPER "${COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB}"
  COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB)
if(NOT COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB MATCHES "^(ON|OFF|AUTO)$")
  message(FATAL_ERROR
    "Invalid COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB="
    "'${COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB}'. "
    "Expected one of: ON, OFF, AUTO.")
endif()

if (NOT APPLE)
  # FIXME: We should support Linux too.
  message(STATUS "Skipping -fbounds-safety runtime tests on non-Apple platforms")
  return()
endif()
set(BOUNDS_SAFETY_LIT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(BOUNDS_SAFETY_HOST_TEST_SUITES)
# Currently no dependencies
set(BOUNDS_SAFETY_TEST_DEPS "")

# Detect LLDB availability and Python compatibility.
#
# Build an ordered list of candidate lldb executables according to
# COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB, then pick the first "usable"
# one. A candidate is usable when `lldb -P` succeeds AND some Python
# interpreter from a per-LLDB candidate list can import the lldb module from
# that path. A Python mismatch (e.g. Homebrew Python 3.13 vs an LLDB built for
# Python 3.11) would otherwise cause a confusing ImportError at test time.
set(_just_built_lldb "${LLVM_TOOLS_BINARY_DIR}/lldb${CMAKE_EXECUTABLE_SUFFIX}")

# Locate a system lldb once; only used for the OFF and AUTO modes below.
find_program(_system_lldb lldb)

set(_lldb_candidates "")
# In ON and AUTO modes the just-built lldb is a (preferred) candidate.
if(NOT COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB STREQUAL "OFF")
  list(APPEND _lldb_candidates "${_just_built_lldb}")
endif()
# In OFF and AUTO modes a system lldb is a candidate (excluding the just-built
# one, which OFF must never consider).
if(NOT COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB STREQUAL "ON"
   AND _system_lldb AND NOT _system_lldb STREQUAL _just_built_lldb)
  list(APPEND _lldb_candidates "${_system_lldb}")
endif()

# Derive the Python interpreter bundled inside the same Xcode `.app` as the
# given LLDB, using the path printed by `lldb -P`. That output looks like
#   <App>.app/Contents/SharedFrameworks/LLDB.framework/Resources/Python
# so walking up four directories yields <App>.app/Contents and the bundled
# interpreter sits at:
#   <App>.app/Contents/Developer/Library/Frameworks/Python3.framework/Versions/Current/bin/python3
# Sets <out_var> to that path if it exists, or to the empty string otherwise.
function(_bounds_safety_xcode_python_from_lldb_resources lldb_resources_path out_var)
  get_filename_component(_resources_dir         "${lldb_resources_path}"    DIRECTORY)
  get_filename_component(_lldb_framework_dir    "${_resources_dir}"         DIRECTORY)
  get_filename_component(_shared_frameworks_dir "${_lldb_framework_dir}"    DIRECTORY)
  get_filename_component(_xcode_contents_dir    "${_shared_frameworks_dir}" DIRECTORY)
  set(_candidate
    "${_xcode_contents_dir}/Developer/Library/Frameworks/Python3.framework/Versions/Current/bin/python3")
  if(EXISTS "${_candidate}")
    set(${out_var} "${_candidate}" PARENT_SCOPE)
  else()
    set(${out_var} "" PARENT_SCOPE)
  endif()
endfunction()

set(_LLDB_DETECTED FALSE)
set(_LLDB_PYTHON_COMPATIBLE FALSE)
set(_indent "  ")
message(STATUS "Detecting LLDB usable for -fbounds-safety tests")
message(STATUS "${_indent}LLDB candidates: ${_lldb_candidates}")
foreach(_cand ${_lldb_candidates})
  message(STATUS "${_indent}Considering LLDB: ${_cand}")
  if(NOT EXISTS "${_cand}")
    message(STATUS "${_indent}${_indent}not found on disk; skipping")
    continue()
  endif()
  execute_process(
    COMMAND ${_cand} -P
    OUTPUT_VARIABLE _cand_py_path
    OUTPUT_STRIP_TRAILING_WHITESPACE
    RESULT_VARIABLE _cand_p_result
  )
  if(NOT _cand_p_result EQUAL 0)
    message(STATUS "${_indent}${_indent}'${_cand} -P' failed (exit ${_cand_p_result}); skipping")
    continue()
  endif()
  set(_LLDB_DETECTED TRUE)
  message(STATUS "${_indent}${_indent}LLDB Python bindings: ${_cand_py_path}")

  # Ordered Python-interpreter candidates for this LLDB; first success wins.
  # When LLDB is from an Xcode bundle, prefer the Python bundled in the same
  # bundle: a different-Xcode Python can succeed at `import lldb` accidentally
  # (when both Xcodes happen to ship the same Python ABI) yet still violate
  # the requirement that bounds-safety tests run against the Python shipped
  # alongside this LLDB. ${Python3_EXECUTABLE} is the fallback for non-Xcode
  # (e.g. just-built) LLDB, where no bundled Python is derivable.
  _bounds_safety_xcode_python_from_lldb_resources("${_cand_py_path}" _xcode_python)
  set(_python_candidates "")
  if(NOT _xcode_python STREQUAL "")
    list(APPEND _python_candidates "${_xcode_python}")
  endif()
  list(APPEND _python_candidates "${Python3_EXECUTABLE}")
  list(REMOVE_DUPLICATES _python_candidates)
  message(STATUS "${_indent}${_indent}Python interpreter candidates: ${_python_candidates}")

  foreach(_py ${_python_candidates})
    message(STATUS "${_indent}${_indent}Trying Python: ${_py}")
    # Tear LLDB down with SBDebugger::Terminate() after a successful import. An
    # assertions-enabled LLDB would otherwise abort at interpreter shutdown.
    execute_process(
      COMMAND ${_py} -c
        "import sys; sys.path.insert(0, '${_cand_py_path}'); import lldb; lldb.SBDebugger.Terminate()"
      RESULT_VARIABLE _LLDB_IMPORT_RESULT
      ERROR_VARIABLE _LLDB_IMPORT_ERROR
      OUTPUT_QUIET
    )
    if(_LLDB_IMPORT_RESULT EQUAL 0)
      message(STATUS "${_indent}${_indent}${_indent}lldb import OK")
      set(BOUNDS_SAFETY_LLDB_PYTHON_PATH "${_cand_py_path}")
      set(BOUNDS_SAFETY_PYTHON_EXECUTABLE "${_py}")
      set(_LLDB_PYTHON_COMPATIBLE TRUE)
      set(_LLDB_EXECUTABLE "${_cand}")
      break() # First usable Python wins for this LLDB.
    else()
      message(STATUS "${_indent}${_indent}${_indent}lldb import FAILED (exit ${_LLDB_IMPORT_RESULT})")
    endif()
  endforeach()
  if(_LLDB_PYTHON_COMPATIBLE)
    break()
  endif()
endforeach()

# Fallback so lit.site.cfg.py.in always gets a valid Python path. The
# LLDB-using suites are gated on COMPILER_RT_BOUNDS_SAFETY_USE_LLDB regardless,
# so a failing detection above does not need to break configure.
if(NOT BOUNDS_SAFETY_PYTHON_EXECUTABLE)
  set(BOUNDS_SAFETY_PYTHON_EXECUTABLE "${Python3_EXECUTABLE}")
endif()

# In ON mode the just-built LLDB is the only candidate. If it isn't usable, warn
# (rather than error) so we preserve COMPILER_RT_BOUNDS_SAFETY_USE_LLDB's graceful
# behavior: it then auto-defaults OFF and disables the debugger suites, unless the
# user explicitly set it ON, in which case the checks below turn this into an error.
if(COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB STREQUAL "ON" AND
   NOT _LLDB_PYTHON_COMPATIBLE)
  message(WARNING
    "COMPILER_RT_BOUNDS_SAFETY_USE_JUST_BUILT_LLDB=ON but the just-built LLDB "
    "(${_just_built_lldb}) is not usable with any of the Python interpreters "
    "tried (${_python_candidates}).\n"
    "This usually means the just-built LLDB was not built (is lldb in "
    "LLVM_ENABLE_PROJECTS and LLDB_ENABLE_PYTHON enabled?) or it targets a "
    "different Python version than any candidate above.\n"
    "Last import error: ${_LLDB_IMPORT_ERROR}")
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
    "COMPILER_RT_BOUNDS_SAFETY_USE_LLDB=ON but none of the Python interpreters "
    "tried (${_python_candidates}) could import the lldb module "
    "from ${BOUNDS_SAFETY_LLDB_PYTHON_PATH}.\n"
    "This typically means Python and LLDB were built for different Python "
    "versions. Either:\n"
    "  - Set Python3_EXECUTABLE to the Python that matches LLDB "
    "(e.g. the one bundled with Xcode), or\n"
    "  - Set COMPILER_RT_BOUNDS_SAFETY_USE_LLDB=OFF to disable debugger "
    "test suites.\n"
    "Last import error: ${_LLDB_IMPORT_ERROR}")
endif()

if (COMPILER_RT_BOUNDS_SAFETY_USE_LLDB)
  message(STATUS "Enabled -fbounds-safety tests using LLDB:\n"
    "LLDB Python bindings: ${BOUNDS_SAFETY_LLDB_PYTHON_PATH}\n"
    "LLDB executable: ${_LLDB_EXECUTABLE}\n"
    "Python interpreter for LLDB Python bindings: ${BOUNDS_SAFETY_PYTHON_EXECUTABLE}")
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
