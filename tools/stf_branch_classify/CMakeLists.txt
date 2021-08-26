project(stf_branch_classify)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_branch_classify stf_branch_classify.cpp)

target_link_libraries(stf_branch_classify ${STF_LINK_LIBS})
