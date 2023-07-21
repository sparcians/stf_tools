project(stf_function_histogram)

include(${STF_TOOLS_CMAKE_DIR}/stf_symbol_table.cmake)

add_executable(stf_function_histogram stf_function_histogram.cpp)

target_link_libraries(stf_function_histogram ${STF_LINK_LIBS} z lzma bz2)
