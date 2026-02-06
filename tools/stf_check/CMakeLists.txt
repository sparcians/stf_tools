project(stf_check)

add_executable(stf_check stf_check.cpp)
target_link_libraries(stf_check stf_decoder stf_tools_libs)
