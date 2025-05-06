include_guard(DIRECTORY)

include(${STF_TOOLS_CMAKE_DIR}/mavis.cmake)
include(${STF_TOOLS_CMAKE_DIR}/stf_elf.cmake)

include(isa_overrides OPTIONAL)

set(STF_LINK_LIBS ${STF_LINK_LIBS} mavis)
