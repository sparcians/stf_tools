project(stf_extract)

add_executable(stf_extract stf_extract.cpp)
target_link_libraries(stf_extract stf_decoder stf_tools_libs)
