set(LLVM_LINK_COMPONENTS
  Support
  TargetParser
  )

add_lld_tool(lld
  lld.cpp

  SUPPORT_PLUGINS
  GENERATE_DRIVER
  )
export_executable_symbols_for_plugins(lld)

function(lld_target_link_libraries target type)
  if (TARGET obj.${target})
    target_link_libraries(obj.${target} ${ARGN})
  endif()

  get_property(LLVM_DRIVER_TOOLS GLOBAL PROPERTY LLVM_DRIVER_TOOLS)
  if(LLVM_TOOL_LLVM_DRIVER_BUILD AND ${target} IN_LIST LLVM_DRIVER_TOOLS)
    set(target llvm-driver)
  endif()

  target_link_libraries(${target} ${type} ${ARGN})
endfunction()

set(LLD_FLAVOR_LIBS lldCommon)
if("COFF" IN_LIST LLD_LINKERS_TO_BUILD)
  list(APPEND LLD_FLAVOR_LIBS lldCOFF)
endif()
if("ELF" IN_LIST LLD_LINKERS_TO_BUILD)
  list(APPEND LLD_FLAVOR_LIBS lldELF)
endif()
if("MachO" IN_LIST LLD_LINKERS_TO_BUILD)
  list(APPEND LLD_FLAVOR_LIBS lldMachO)
endif()
if("MinGW" IN_LIST LLD_LINKERS_TO_BUILD)
  list(APPEND LLD_FLAVOR_LIBS lldMinGW)
endif()
if("Wasm" IN_LIST LLD_LINKERS_TO_BUILD)
  list(APPEND LLD_FLAVOR_LIBS lldWasm)
endif()

lld_target_link_libraries(lld PRIVATE ${LLD_FLAVOR_LIBS})

if(NOT LLD_SYMLINKS_TO_CREATE)
  set(LLD_SYMLINKS_TO_CREATE "")
  if("COFF" IN_LIST LLD_LINKERS_TO_BUILD)
    list(APPEND LLD_SYMLINKS_TO_CREATE lld-link)
  endif()
  if("ELF" IN_LIST LLD_LINKERS_TO_BUILD)
    list(APPEND LLD_SYMLINKS_TO_CREATE ld.lld)
  endif()
  if("MachO" IN_LIST LLD_LINKERS_TO_BUILD)
    list(APPEND LLD_SYMLINKS_TO_CREATE ld64.lld)
  endif()
  if("Wasm" IN_LIST LLD_LINKERS_TO_BUILD)
    list(APPEND LLD_SYMLINKS_TO_CREATE wasm-ld)
  endif()
endif()

foreach(link ${LLD_SYMLINKS_TO_CREATE})
  add_lld_symlink(${link} lld)
endforeach()

if(LLVM_TOOL_LLVM_DRIVER_BUILD)
  set_property(GLOBAL APPEND PROPERTY LLVM_DRIVER_HIDDEN_TOOL_ALIASES_lld ld)
endif()
