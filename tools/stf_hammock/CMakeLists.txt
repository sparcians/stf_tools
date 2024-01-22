project(stf_hammock)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)
include(${STF_TOOLS_CMAKE_DIR}/stf_elf.cmake)
include(${STF_TOOLS_CMAKE_DIR}/disassembler.cmake)

add_executable(stf_hammock stf_hammock.cpp)

target_link_libraries(stf_hammock ${STF_LINK_LIBS} z)
