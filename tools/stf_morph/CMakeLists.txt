project(stf_morph)

include(${STF_CMAKE_DIR}/stf_decoder.cmake)

add_executable(stf_morph stf_morph.cpp stf_morpher.cpp)

target_link_libraries(stf_morph ${STF_LINK_LIBS})
