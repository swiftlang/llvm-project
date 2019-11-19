file(GLOB SWIG_INTERFACES interface/*.i)
file(GLOB_RECURSE SWIG_SOURCES *.swig)
set(SWIG_HEADERS
  ${LLDB_SOURCE_DIR}/include/lldb/API/SBDefines.h
  ${LLDB_SOURCE_DIR}/include/lldb/lldb-defines.h
  ${LLDB_SOURCE_DIR}/include/lldb/lldb-enumerations.h
  ${LLDB_SOURCE_DIR}/include/lldb/lldb-forward.h
  ${LLDB_SOURCE_DIR}/include/lldb/lldb-types.h
  ${LLDB_SOURCE_DIR}/include/lldb/lldb-versioning.h
)

if(LLDB_BUILD_FRAMEWORK)
  set(framework_arg --framework --target-platform Darwin)
endif()

if(${LLDB_USE_STATIC_BINDINGS})
  set(PREPARE_BINDINGS_ARGS
    --use-static-binding)
else()
  find_package(SWIG)
  if(${SWIG_FOUND})
    set(SWIG_MIN_VERSION "2.0.0")
    if (${SWIG_VERSION} VERSION_LESS ${SWIG_MIN_VERSION})
      message(FATAL_ERROR "LLDB requires swig ${SWIG_MIN_VERSION}, your version is ${SWIG_VERSION}.")
    endif()
    set(PREPARE_BINDINGS_ARGS
      "--swig-executable=${SWIG_EXECUTABLE}")
  else()
    message(FATAL_ERROR "LLDB requires swig ${SWIG_MIN_VERSION}. To use the static bindings instead use -DLLDB_USE_STATIC_BINDINGS=ON")
  endif()
endif()

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/LLDBWrapPython.cpp
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/lldb.py
  DEPENDS ${SWIG_SOURCES}
  DEPENDS ${SWIG_INTERFACES}
  DEPENDS ${SWIG_HEADERS}
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Python/prepare_binding_Python.py
  COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/prepare_bindings.py
      ${framework_arg}
      --srcRoot=${LLDB_SOURCE_DIR}
      --targetDir=${CMAKE_CURRENT_BINARY_DIR}
      --cfgBldDir=${CMAKE_CURRENT_BINARY_DIR}
      --prefix=${CMAKE_BINARY_DIR}
      ${PREPARE_BINDINGS_ARGS}
  VERBATIM
  COMMENT "Python script building LLDB Python wrapper")

add_custom_target(swig_wrapper ALL DEPENDS
  ${CMAKE_CURRENT_BINARY_DIR}/LLDBWrapPython.cpp
  ${CMAKE_CURRENT_BINARY_DIR}/lldb.py
)

if(NOT LLDB_BUILD_FRAMEWORK)
  if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(swig_python_subdir site-packages)
  else()
    set(swig_python_subdir python${PYTHON_VERSION_MAJOR}.${PYTHON_VERSION_MINOR})
  endif()

  set(SWIG_PYTHON_DIR ${LLVM_LIBRARY_OUTPUT_INTDIR}/${swig_python_subdir})
  set(SWIG_INSTALL_DIR lib${LLVM_LIBDIR_SUFFIX})

  # Install the LLDB python module
  # SWIFT_ENABLE_TENSORFLOW: Install in liblldb component so that it gets included in the toolchain.
  # TODO: There are some upstream changes that install python in a "lldb-python-scripts" component.
  #       (f6bedd81cca5c1a60ef5faa5a5e9cb3d5737ffdd, 61f471a705a5df3d581ba4905337f433bac3ba1f, and more).
  #       When merging those changes, we should delete the tensorflow-branch changes, and add
  #       "lldb-python-scripts" to "lldb/cmake/caches/Apple-lldb-Linux.cmake".
  install(DIRECTORY ${SWIG_PYTHON_DIR} DESTINATION ${SWIG_INSTALL_DIR} COMPONENT liblldb)
  
  # SWIFT_ENABLE_TENSORFLOW
  # The build might have generated some "python3.x" symlinks. See the comment in
  # "finishSwigPythonLLDB.py" for more information. Install them, if they exist.
  install(DIRECTORY "${CMAKE_BINARY_DIR}/lib${LLVM_LIBDIR_SUFFIX}/python3.6" DESTINATION ${SWIG_INSTALL_DIR} OPTIONAL COMPONENT liblldb)
  install(DIRECTORY "${CMAKE_BINARY_DIR}/lib${LLVM_LIBDIR_SUFFIX}/python3.7" DESTINATION ${SWIG_INSTALL_DIR} OPTIONAL COMPONENT liblldb)
endif()
