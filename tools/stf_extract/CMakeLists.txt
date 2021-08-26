project(stf_extract)

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_extract stf_extract.cpp)

target_link_libraries(stf_extract ${STF_LINK_LIBS})
