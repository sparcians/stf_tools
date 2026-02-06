project(stf_isa_edit)

add_executable(stf_isa_edit stf_isa_edit.cpp)
target_link_libraries(stf_isa_edit stf_decoder stf_tools_libs)
