include(GNUInstallDirs)

# Create sphinx target
if (LLVM_ENABLE_SPHINX)
  message(STATUS "Sphinx enabled.")
  find_package(Sphinx REQUIRED)

  # Sphinx has internal parallelism, so give it a custom job pool. Sphinx tends
  # not to use all available CPU, so this is not enabled by default.
  set(LLVM_PARALLEL_SPHINX_JOBS "" CACHE STRING
    "Define the maximum number of concurrent sphinx-build invocations (Ninja only).")
  if (LLVM_PARALLEL_SPHINX_JOBS)
    if (CMAKE_GENERATOR MATCHES "Ninja")
      get_property(_sphinx_job_pools GLOBAL PROPERTY JOB_POOLS)
      if (NOT "sphinx_job_pool=${LLVM_PARALLEL_SPHINX_JOBS}" IN_LIST _sphinx_job_pools)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS sphinx_job_pool=${LLVM_PARALLEL_SPHINX_JOBS})
      endif()
      set(sphinx_job_pool JOB_POOL sphinx_job_pool)
    else()
      message(WARNING "Job pooling is only available with Ninja generators.")
    endif()
  endif()

  if (LLVM_BUILD_DOCS AND NOT TARGET sphinx)
    add_custom_target(sphinx ALL)
    set_target_properties(sphinx PROPERTIES FOLDER "LLVM/Docs")
  endif()
else()
  message(STATUS "Sphinx disabled.")
endif()


# Handy function for creating the different Sphinx targets.
#
# ``builder`` should be one of the supported builders used by
# the sphinx-build command.
#
# ``project`` should be the project name
#
# Named arguments:
# ``ENV_VARS`` should be a list of environment variables that should be set when
#              running Sphinx. Each environment variable should be a string with
#              the form KEY=VALUE.
function (add_sphinx_target builder project)
  cmake_parse_arguments(ARG "" "SOURCE_DIR" "ENV_VARS" ${ARGN})
  set(SPHINX_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/${builder}")
  set(SPHINX_DOC_TREE_DIR "${CMAKE_CURRENT_BINARY_DIR}/_doctrees-${project}-${builder}")
  set(SPHINX_TARGET_NAME docs-${project}-${builder})

  if (SPHINX_WARNINGS_AS_ERRORS)
    set(SPHINX_WARNINGS_AS_ERRORS_FLAG "-W")
  else()
    set(SPHINX_WARNINGS_AS_ERRORS_FLAG "")
  endif()

  if (NOT ARG_SOURCE_DIR)
    set(ARG_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
  endif()

  # Give Sphinx some internal job parallelism, since it tends to be on the
  # critical path at the end of the build. This can speed up doc builds by
  # ~80%. Sphinx rarely consumes all cores available, so it's safe to
  # overallocate a bit.
  if (NOT DEFINED LLVM_SPHINX_THREADS)
    cmake_host_system_information(RESULT number_of_logical_cores
                                  QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR LLVM_SPHINX_THREADS
         "(${number_of_logical_cores} + 1) / 2")
  endif()
  set(LLVM_SPHINX_THREADS "${LLVM_SPHINX_BUILD_JOBS}"
      CACHE STRING "Define the number of parallel jobs for each Sphinx build.")
  if (LLVM_SPHINX_THREADS)
    set(sphinx_jobs_flag -j ${LLVM_SPHINX_THREADS})
  endif()

  if ("${LLVM_VERSION_SUFFIX}" STREQUAL "git")
    set(PreReleaseTag "-tPreRelease")
  endif()

  add_custom_target(${SPHINX_TARGET_NAME}
                    COMMAND ${CMAKE_COMMAND} -E env ${ARG_ENV_VARS}
                            --modify "PYTHONPATH=path_list_append:${LLVM_MAIN_SRC_DIR}/../utils/docs"
                            ${SPHINX_EXECUTABLE}
                            -b ${builder}
                            -d "${SPHINX_DOC_TREE_DIR}"
                            ${sphinx_jobs_flag}
                            -q # Quiet: no output other than errors and warnings.
                            -t builder-${builder} # tag for builder
                            -D version=${LLVM_VERSION_MAJOR}
                            -D release=${PACKAGE_VERSION}
                            ${PreReleaseTag}
                            ${SPHINX_WARNINGS_AS_ERRORS_FLAG} # Treat warnings as errors if requested
                            "${ARG_SOURCE_DIR}" # Source
                            "${SPHINX_BUILD_DIR}" # Output
                    ${sphinx_job_pool}
                    COMMENT
                    "Generating ${builder} Sphinx documentation for ${project} into \"${SPHINX_BUILD_DIR}\"")
  get_subproject_title(subproject_title)
  set_target_properties(${SPHINX_TARGET_NAME} PROPERTIES FOLDER "${subproject_title}/Docs")

  # When "clean" target is run, remove the Sphinx build directory
  set_property(DIRECTORY APPEND PROPERTY
               ADDITIONAL_MAKE_CLEAN_FILES
               "${SPHINX_BUILD_DIR}")

  # We need to remove ${SPHINX_DOC_TREE_DIR} when make clean is run
  # but we should only add this path once
  get_property(_CURRENT_MAKE_CLEAN_FILES
               DIRECTORY PROPERTY ADDITIONAL_MAKE_CLEAN_FILES)
  if (NOT "${SPHINX_DOC_TREE_DIR}" IN_LIST _CURRENT_MAKE_CLEAN_FILES)
    set_property(DIRECTORY APPEND PROPERTY
                 ADDITIONAL_MAKE_CLEAN_FILES
                 "${SPHINX_DOC_TREE_DIR}")
  endif()

  if (LLVM_BUILD_DOCS)
    add_dependencies(sphinx ${SPHINX_TARGET_NAME})

    # Handle installation
    if (NOT LLVM_INSTALL_TOOLCHAIN_ONLY)
      if (builder STREQUAL man)
        # FIXME: We might not ship all the tools that these man pages describe
        install(DIRECTORY "${SPHINX_BUILD_DIR}/" # Slash indicates contents of
                COMPONENT "${project}-sphinx-man"
                DESTINATION "${CMAKE_INSTALL_MANDIR}/man1")

        if(NOT LLVM_ENABLE_IDE)
          add_llvm_install_targets("install-${SPHINX_TARGET_NAME}"
                                   DEPENDS ${SPHINX_TARGET_NAME}
                                   COMPONENT "${project}-sphinx-man")
        endif()
      elseif (builder STREQUAL html)
        string(TOUPPER "${project}" project_upper)
        set(${project_upper}_INSTALL_SPHINX_HTML_DIR "${CMAKE_INSTALL_DOCDIR}/${project}/html"
            CACHE STRING "HTML documentation install directory for ${project}")

        # '/.' indicates: copy the contents of the directory directly into
        # the specified destination, without recreating the last component
        # of ${SPHINX_BUILD_DIR} implicitly.
        install(DIRECTORY "${SPHINX_BUILD_DIR}/."
                COMPONENT "${project}-sphinx-html"
                DESTINATION "${${project_upper}_INSTALL_SPHINX_HTML_DIR}")

        if(NOT LLVM_ENABLE_IDE)
          add_llvm_install_targets("install-${SPHINX_TARGET_NAME}"
                                   DEPENDS ${SPHINX_TARGET_NAME}
                                   COMPONENT "${project}-sphinx-html")
        endif()
      else()
        message(WARNING Installation of ${builder} not supported)
      endif()
    endif()
  endif()
endfunction()
