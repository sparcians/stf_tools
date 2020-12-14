set (STF_TOOL_DIR ${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_MODULE_PATH ${STF_BASE}/cmake ${CMAKE_MODULE_PATH})
include(GetGitRevisionDescription)
get_git_head_revision(GIT_REFSPEC TRACE_TOOLS_GIT_SHA1)
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/trace_tools_git_version.cpp.in" "${CMAKE_CURRENT_BINARY_DIR}/trace_tools_git_version.cpp" @ONLY)

add_library(trace_tools_version "${CMAKE_CURRENT_BINARY_DIR}/trace_tools_git_version.cpp")

set (STF_LINK_LIBS ${STF_LINK_LIBS} trace_tools_version stdc++)

set(FILESYSTEM_CHECK_FLAGS ${CMAKE_CXX_FLAGS})
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(FILESYSTEM_CHECK_FLAGS "${FILESYSTEM_CHECK_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

execute_process(COMMAND bash "-c" "${CMAKE_CXX_COMPILER} ${FILESYSTEM_CHECK_FLAGS} -E ${STF_TOOL_DIR}/tools-inc/filesystem.hpp | grep \"namespace fs\" | grep -qv experimental"
                RESULT_VARIABLE FILESYSTEM_EXPERIMENTAL)

if (FILESYSTEM_EXPERIMENTAL EQUAL 1)
    SET(STF_LINK_LIBS ${STF_LINK_LIBS} stdc++fs)
endif()

include_directories(${STF_BASE}/stf-inc)
include_directories(${STF_INCLUDE_DIRS} ${STF_TOOL_DIR}/tools-inc)

include(${STF_TOOL_DIR}/FindDisassemblers.cmake)

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

if(STF_INSTALL_DIR)
    install(TARGETS stf_dump stf_check stf_extract stf_trace_info stf_count stf_find stf_verify_pc stf_merge stf_filter_evt stf_imem stf_imix stf_bench stf_recompress stf_count_multi_process stf_record_dump stf_strip stf_address_map stf_branch_classify stf_bbv stf_diff stf_morph
            DESTINATION ${STF_INSTALL_DIR})
endif()
