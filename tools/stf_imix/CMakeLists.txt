project(stf_imix)

add_executable(stf_imix stf_imix.cpp)
target_link_libraries(stf_imix stf_decoder stf_tools_libs)
