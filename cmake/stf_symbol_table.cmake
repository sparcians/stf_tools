include(${STF_TOOLS_CMAKE_DIR}/stf_elf.cmake)
include_directories(${LIBDWARF_INCLUDE_DIRS}/libdwarf-0)
set(STF_LINK_LIBS ${STF_LINK_LIBS} libdwarf)
