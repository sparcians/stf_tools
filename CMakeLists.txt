cmake_minimum_required(VERSION 3.24)
project (stf_tools C CXX)

cmake_policy(SET CMP0135 NEW)
set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")
endif()

set(STF_TOOLS_BASE ${CMAKE_CURRENT_SOURCE_DIR})
set(STF_TOOLS_CMAKE_DIR ${STF_TOOLS_BASE}/cmake)
set(STF_TOOLS_PATCHES_DIR ${STF_TOOLS_BASE}/patches)
set(STF_LIB_CMAKE_DIR ${STF_TOOLS_BASE}/stf_lib/cmake)

set(CMAKE_MODULE_PATH ${STF_TOOLS_CMAKE_DIR} ${STF_LIB_CMAKE_DIR} ${CMAKE_MODULE_PATH})

if(NOT EXISTS ${STF_TOOLS_BASE}/stf_lib/.git)
    message(FATAL_ERROR "stf_lib hasn't been checked out. Please run git submodule update --init --recursive")
endif()

add_compile_options(-Werror -fPIC -g -Wall -Wextra -pedantic -Wconversion -Wno-unused-parameter -Wno-unused-function -pipe)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-Wno-gnu-zero-variadic-macro-arguments)

    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.0)
        add_compile_options(-Wno-c++20-extensions)
    endif()
endif()

include(std_filesystem)

include(binutils)

include(libdwarf)

set (DISABLE_STF_DOXYGEN ON)
set (DISABLE_STF_TESTS ON)

if(NOT STF_INSTALL_DIR)
    set(STF_INSTALL_DIR bin)
endif()

if(NOT DISABLE_STF_PYTHON_LIB)
    if(NOT STFPY_INSTALL_DIR)
        set(STFPY_INSTALL_DIR ${STF_INSTALL_DIR})
    endif()

    set(BUILD_STF_PYTHON_LIB ON)
endif()

add_subdirectory(stf_lib)

add_library(stf_tools_base INTERFACE)
target_include_directories(stf_tools_base INTERFACE ${STF_TOOLS_BASE}/include)
target_link_libraries(stf_tools_base INTERFACE stf)

add_subdirectory(lib)

include(mavis)
include(dtl)
include(rapidjson)
include(stf_decoder)
include(disassembler)
include(hdf5)
include(stf_symbol_table)

add_subdirectory(tools)

add_subdirectory(examples)

set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_DIRECTORY ${PROJECT_BINARY_DIR}/package)

include(CPack)
