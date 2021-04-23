project(stf_check)

include(${STF_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_check stf_check.cpp)

target_link_libraries(stf_check ${STF_LINK_LIBS})
