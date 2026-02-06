project(stf_cstar)

add_executable(stf_cstar stf_cstar.cpp)
target_link_libraries(stf_cstar stf_disasm stf_tools_libs)
