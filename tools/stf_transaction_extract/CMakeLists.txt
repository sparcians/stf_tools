project(stf_transaction_extract)

add_executable(stf_transaction_extract stf_transaction_extract.cpp)

target_link_libraries(stf_transaction_extract ${STF_LINK_LIBS})
