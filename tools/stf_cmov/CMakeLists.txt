project(stf_cmov)

add_executable(stf_cmov stf_cmov.cpp)
target_link_libraries(stf_cmov stf_disasm stf_tools_libs)
