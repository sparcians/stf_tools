project(stf_verify_pc)

add_executable(stf_verify_pc stf_verify_pc.cpp)

target_link_libraries(stf_verify_pc ${STF_LINK_LIBS})
