project(stf_hammock)

add_executable(stf_hammock stf_hammock.cpp)
target_link_libraries(stf_hammock stf_disasm stf_tools_libs rapidjson)
