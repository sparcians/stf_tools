set (STF_TOOL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

include(GitVersionGenerator)
git_version_generator("${CMAKE_CURRENT_SOURCE_DIR}/trace_tools_git_version.cpp.in"
                      "${CMAKE_CURRENT_BINARY_DIR}/trace_tools_git_version.cpp"
                      "TRACE_TOOLS_GIT_SHA1")

add_library(trace_tools_version "${CMAKE_CURRENT_BINARY_DIR}/trace_tools_git_version.cpp")

set (STF_LINK_LIBS ${EXTRA_LIBS} ${STF_LINK_LIBS} trace_tools_version stdc++)

add_subdirectory(stf_dump)
add_subdirectory(stf_check)
add_subdirectory(stf_extract)
add_subdirectory(stf_trace_info)
add_subdirectory(stf_count)
add_subdirectory(stf_find)
add_subdirectory(stf_verify_pc)
add_subdirectory(stf_merge)
add_subdirectory(stf_filter_evt)
add_subdirectory(stf_imem)
add_subdirectory(stf_imix)
add_subdirectory(stf_bench)
add_subdirectory(stf_recompress)
add_subdirectory(stf_count_multi_process)
add_subdirectory(stf_record_dump)
add_subdirectory(stf_strip)
add_subdirectory(stf_address_map)
add_subdirectory(stf_branch_classify)
add_subdirectory(stf_bbv)
add_subdirectory(stf_diff)
add_subdirectory(stf_morph)
add_subdirectory(stf_branch_hdf5)
add_subdirectory(stf_branch_dump)
add_subdirectory(stf_transaction_dump)
add_subdirectory(stf_transaction_extract)
add_subdirectory(stf_function_histogram)
add_subdirectory(stf_st_ld)
add_subdirectory(stf_cmov)
add_subdirectory(stf_ld_ld)
add_subdirectory(stf_ls_access_map)
add_subdirectory(stf_tuple)
add_subdirectory(stf_hammock)
add_subdirectory(stf_cstar)
add_subdirectory(stf_address_sequence)
add_subdirectory(stf_branch_lhr)
add_subdirectory(stf_branch_correlator)
add_subdirectory(stf_disable_feature)

set(STF_INSTALL_TARGETS
    stf_dump
    stf_check
    stf_extract
    stf_trace_info
    stf_count
    stf_find
    stf_verify_pc
    stf_merge
    stf_filter_evt
    stf_imem
    stf_imix
    stf_bench
    stf_recompress
    stf_count_multi_process
    stf_record_dump
    stf_strip
    stf_address_map
    stf_branch_classify
    stf_bbv
    stf_diff
    stf_morph
    stf_branch_hdf5
    stf_branch_dump
    stf_transaction_dump
    stf_transaction_extract
    stf_function_histogram
    stf_st_ld
    stf_cmov
    stf_ld_ld
    stf_ls_access_map
    stf_tuple
    stf_hammock
    stf_cstar
    stf_address_sequence
    stf_branch_lhr
    stf_branch_correlator
    stf_disable_feature
)

include(stf_extra_tools.cmake OPTIONAL)

if(STF_INSTALL_DIR)
    install(TARGETS ${STF_INSTALL_TARGETS}
            DESTINATION ${STF_INSTALL_DIR})
endif()
