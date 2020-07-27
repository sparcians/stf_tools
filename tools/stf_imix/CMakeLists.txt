project(stf_imix)

include(${STF_TOOL_DIR}/mavis.cmake)

add_executable(stf_imix stf_imix.cpp)

target_link_libraries(stf_imix ${STF_LINK_LIBS})
