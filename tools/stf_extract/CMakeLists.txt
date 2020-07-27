project(stf_extract)

include(${STF_TOOL_DIR}/mavis.cmake)

add_executable(stf_extract stf_extract.cpp)

target_link_libraries(stf_extract ${STF_LINK_LIBS})
