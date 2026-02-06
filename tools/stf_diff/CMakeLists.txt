project(stf_diff)

add_executable(stf_diff stf_diff.cpp)
target_link_libraries(stf_diff stf_disasm dtl stf_tools_libs)
