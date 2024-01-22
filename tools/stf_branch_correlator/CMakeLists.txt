project(stf_branch_correlator)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_branch_correlator stf_branch_correlator.cpp)

target_link_libraries(stf_branch_correlator ${STF_LINK_LIBS})
