project(stf_cstar)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)
include(${STF_TOOLS_CMAKE_DIR}/stf_elf.cmake)
include(${STF_TOOLS_CMAKE_DIR}/disassembler.cmake)

add_executable(stf_cstar stf_cstar.cpp)

target_link_libraries(stf_cstar ${STF_LINK_LIBS} z)
