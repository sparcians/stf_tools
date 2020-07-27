project(stf_strip)

add_executable(stf_strip stf_strip.cpp)

target_link_libraries(stf_strip ${STF_LINK_LIBS})
