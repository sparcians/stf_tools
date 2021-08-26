project(stf_diff)

include(${STF_TOOLS_CMAKE_DIR}/disassembler.cmake)
include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)
include(${STF_TOOLS_CMAKE_DIR}/dtl.cmake)

add_executable(stf_diff stf_diff.cpp)

target_link_libraries(stf_diff ${STF_LINK_LIBS})
