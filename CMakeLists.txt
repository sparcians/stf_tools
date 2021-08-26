cmake_minimum_required(VERSION 3.14)
project (stf_tools CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package (Boost 1.49.0)
include_directories (SYSTEM ${Boost_INCLUDE_DIRS})

set (STF_TOOLS_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${STF_TOOLS_BASE}/include)
set(STF_TOOLS_CMAKE_DIR ${STF_TOOLS_BASE}/cmake)
set(STF_LIB_CMAKE_DIR ${STF_TOOLS_BASE}/stf_lib/cmake)

if(NOT EXISTS ${STF_TOOLS_BASE}/stf_lib/.git)
    message(FATAL_ERROR "stf_lib hasn't been checked out. Please run git submodule update --init --recursive")
endif()

add_compile_options(-Werror -std=c++17 -fPIC -Wall -Wextra -pedantic -Wconversion -Wno-unused-parameter -Wno-unused-function -pipe)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-mllvm -inline-threshold=1024)
endif()

include(${STF_TOOLS_CMAKE_DIR}/std_filesystem.cmake)

set(FULL_LTO 1)

include(${STF_LIB_CMAKE_DIR}/stf_linker_setup.cmake)

set (DISABLE_STF_DOXYGEN ON)

add_subdirectory(stf_lib)

include_directories(${STF_BASE}/stf-inc)

add_subdirectory(generators)

add_subdirectory(tools)
