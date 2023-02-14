cmake_minimum_required(VERSION 3.17)
project (stf_tools CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package (Boost 1.49.0)
include_directories (SYSTEM ${Boost_INCLUDE_DIRS})

set (STF_TOOLS_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${STF_TOOLS_BASE}/include)
set(STF_TOOLS_CMAKE_DIR ${STF_TOOLS_BASE}/cmake)
set(STF_TOOLS_PATCHES_DIR ${STF_TOOLS_BASE}/patches)
set(STF_LIB_CMAKE_DIR ${STF_TOOLS_BASE}/stf_lib/cmake)

set(CMAKE_MODULE_PATH ${STF_TOOLS_CMAKE_DIR} ${STF_LIB_CMAKE_DIR} ${CMAKE_MODULE_PATH})

if(NOT EXISTS ${STF_TOOLS_BASE}/stf_lib/.git)
    message(FATAL_ERROR "stf_lib hasn't been checked out. Please run git submodule update --init --recursive")
endif()

add_compile_options(-Werror -std=c++17 -fPIC -Wall -Wextra -pedantic -Wconversion -Wno-unused-parameter -Wno-unused-function -pipe)

include(std_filesystem)

include(stf_linker_setup)

include(binutils)

set(FULL_LTO true)

setup_stf_linker(true)

set (DISABLE_STF_DOXYGEN ON)

set(BUILD_STF_PYTHON_LIB ON)

add_subdirectory(stf_lib)

include_directories(${STF_BASE}/stf-inc)

add_subdirectory(generators)

add_subdirectory(lib)

add_subdirectory(tools)

add_subdirectory(examples)
