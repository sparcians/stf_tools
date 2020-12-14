project(stf_record_dump)

add_executable(stf_record_dump stf_record_dump.cpp)

target_link_libraries(stf_record_dump ${STF_LINK_LIBS})
