project(stf_filter_evt)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_filter_evt stf_filter_evt.cpp)

target_link_libraries(stf_filter_evt ${STF_LINK_LIBS})
