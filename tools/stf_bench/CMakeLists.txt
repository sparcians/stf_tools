project(stf_bench)

add_executable(stf_bench stf_bench.cpp)

target_link_libraries(stf_bench ${STF_LINK_LIBS})
