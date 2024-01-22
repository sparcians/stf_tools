project(stf_tuple)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_tuple stf_tuple.cpp)

target_link_libraries(stf_tuple ${STF_LINK_LIBS})
