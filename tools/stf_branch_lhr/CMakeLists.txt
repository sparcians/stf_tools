project(stf_branch_lhr)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_branch_lhr stf_branch_lhr.cpp)

target_link_libraries(stf_branch_lhr ${STF_LINK_LIBS})
