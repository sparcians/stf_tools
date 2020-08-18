project(stf_diff)

include(${STF_TOOL_DIR}/disassembler.cmake)
include(${STF_TOOL_DIR}/mavis.cmake)
include(${STF_TOOL_DIR}/dtl.cmake)

add_executable(stf_diff stf_diff.cpp)

target_link_libraries(stf_diff ${STF_LINK_LIBS})
