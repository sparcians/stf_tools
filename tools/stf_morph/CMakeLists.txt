project(stf_morph)

add_executable(stf_morph stf_morph.cpp stf_morpher.cpp)
target_link_libraries(stf_morph stf_decoder stf_tools_libs)
