project(stf_dump)

add_executable(stf_dump stf_dump.cpp)
target_link_libraries(stf_dump stf_disasm stf_tools_libs)
