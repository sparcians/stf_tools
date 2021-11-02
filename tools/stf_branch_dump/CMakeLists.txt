project(stf_branch_dump)

add_executable(stf_branch_dump stf_branch_dump.cpp)

target_link_libraries(stf_branch_dump ${STF_LINK_LIBS})
