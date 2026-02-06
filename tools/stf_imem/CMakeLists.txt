project(stf_imem)

add_executable(stf_imem stf_imem.cpp)
target_link_libraries(stf_imem stf_disasm stf_tools_libs)
