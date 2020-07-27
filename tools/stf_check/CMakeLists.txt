project(stf_check)

include(${STF_TOOL_DIR}/mavis.cmake)

add_executable(stf_check stf_check.cpp)

target_link_libraries(stf_check ${STF_LINK_LIBS})
