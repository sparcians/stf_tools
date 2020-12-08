project(stf_morph)

include(${STF_TOOL_DIR}/mavis.cmake)

add_executable(stf_morph stf_morph.cpp)

target_link_libraries(stf_morph ${STF_LINK_LIBS})
