project(stf_isa_edit)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_isa_edit stf_isa_edit.cpp)

target_link_libraries(stf_isa_edit ${STF_LINK_LIBS})
