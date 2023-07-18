project(stf_function_histogram)

ExternalProject_Get_Property(libdwarf install_dir)
find_package(PkgConfig REQUIRED)

include_directories(${BINUTILS_INCLUDE_DIRS})
include_directories(${install_dir}/include/libdwarf-0)

include(${STF_TOOLS_CMAKE_DIR}/elfio.cmake)

set(STF_LINK_LIBS ${STF_LINK_LIBS} binutils_wrapper ${install_dir}/lib/libdwarf.a)

add_executable(stf_function_histogram stf_function_histogram.cpp)
add_dependencies(stf_function_histogram libdwarf)

target_link_libraries(stf_function_histogram ${STF_LINK_LIBS} z lzma bz2)
