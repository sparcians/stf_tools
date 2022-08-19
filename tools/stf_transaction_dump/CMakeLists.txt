project(stf_transaction_dump)

add_executable(stf_transaction_dump stf_transaction_dump.cpp)

target_link_libraries(stf_transaction_dump ${STF_LINK_LIBS})
