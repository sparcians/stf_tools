project(stf_cmov)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)
include(${STF_TOOLS_CMAKE_DIR}/stf_elf.cmake)
include(${STF_TOOLS_CMAKE_DIR}/disassembler.cmake)

add_executable(stf_cmov stf_cmov.cpp)

target_link_libraries(stf_cmov ${STF_LINK_LIBS} z)
