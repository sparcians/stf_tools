cmake_minimum_required(VERSION 3.24)
project (stf_tools C CXX)

cmake_policy(SET CMP0135 NEW)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_compile_options(-Werror -std=c++17 -fPIC -Wall -Wextra -pedantic -Wconversion -Wno-unused-parameter -Wno-unused-function -Wno-gnu-zero-variadic-macro-arguments -pipe)

include(std_filesystem)

include(stf_linker_setup)

include(binutils)

include(libdwarf)

set(FULL_LTO true)

setup_stf_linker(true)

set (DISABLE_STF_DOXYGEN ON)

if(NOT DISABLE_STF_PYTHON_LIB)
    if(NOT STFPY_INSTALL_DIR)
        set(STFPY_INSTALL_DIR ${STF_INSTALL_DIR})
    endif()

    set(BUILD_STF_PYTHON_LIB ON)
endif()

add_subdirectory(stf_lib)

include_directories(${STF_BASE}/stf-inc)

include(${STF_TOOLS_CMAKE_DIR}/mavis_setup.cmake)

add_subdirectory(generators)

add_subdirectory(lib)

add_subdirectory(tools)

add_subdirectory(examples)
