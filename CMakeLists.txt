cmake_minimum_required(VERSION 3.14)
project (stf_tools CXX)

set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

# Boost
find_package (Boost 1.49.0)
include_directories (SYSTEM ${Boost_INCLUDE_DIRS})

set (STF_TOOLS_BASE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${STF_TOOLS_BASE}/include)
set(STF_CMAKE_DIR ${STF_TOOLS_BASE}/cmake)

set(FILESYSTEM_CHECK_FLAGS ${CMAKE_CXX_FLAGS})
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(FILESYSTEM_CHECK_FLAGS "${FILESYSTEM_CHECK_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

execute_process(COMMAND bash "-c" "${CMAKE_CXX_COMPILER} ${FILESYSTEM_CHECK_FLAGS} -E ${STF_TOOLS_BASE}/include/filesystem.hpp | grep \"namespace fs\" | grep -qv experimental"
                RESULT_VARIABLE FILESYSTEM_EXPERIMENTAL)

if (FILESYSTEM_EXPERIMENTAL EQUAL 1)
    SET(EXTRA_LIBS ${EXTRA_LIBS} stdc++fs)
endif()

if(NOT EXISTS ${STF_TOOLS_BASE}/stf_lib/.git)
    message(FATAL_ERROR "stf_lib hasn't been checked out. Please run git submodule update --init --recursive")
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OPT_FLAGS} \
 -Werror -std=c++17 -fPIC \
 -Wall -Wextra -pedantic -Wconversion -Wno-unused-parameter -Wno-unused-function -pipe")
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-mllvm -inline-threshold=1024)
endif()

if(FULL_LTO)
    SET(LTO_FLAGS "-flto")
else()
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        SET(LTO_FLAGS "-flto=thin")
    else()
        SET(LTO_FLAGS "-flto")
    endif()
endif()

# Don't need to change default linker on OS X
if (NOT CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    find_program(GOLD "ld.gold")
    find_program(LLD "ld.lld")

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(LLD)
            SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
        elseif(GOLD)
            SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
        else()
            message(FATAL_ERROR "Either ld.lld or ld.gold are required when compiling with clang")
        endif()
    else()
        if(GOLD)
            SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")
        else()
            message(FATAL_ERROR "ld.gold is required when compiling with gcc")
        endif()
    endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O0 -g -pipe -fno-omit-frame-pointer")
elseif (CMAKE_BUILD_TYPE MATCHES "^[Pp]rofile")
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3 -g -pipe -fomit-frame-pointer ${LTO_FLAGS}")
else()
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -O3 -pipe -fomit-frame-pointer ${LTO_FLAGS}")
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  if(APPLE)
    SET(CMAKE_AR "ar")
  else()
    unset(LLVM_AR)
    unset(LLVM_AR CACHE)
    # using regular Clang or AppleClang
    find_program(LLVM_AR "llvm-ar")
    if (NOT LLVM_AR)
      unset(LLVM_AR)
      unset(LLVM_AR CACHE)
      find_program(LLVM_AR "llvm-ar-9")
      if (NOT LLVM_AR)
        message(FATAL_ERROR "llvm-ar is needed to link trace_tools on this system")
      else()
        SET(CMAKE_AR "llvm-ar-9")
      endif()
    else()
      SET(CMAKE_AR "llvm-ar")
    endif()
  endif()
else ()
  SET(CMAKE_AR  "gcc-ar")
endif()
SET(CMAKE_CXX_ARCHIVE_CREATE "<CMAKE_AR> qcs <TARGET> <LINK_FLAGS> <OBJECTS>")
SET(CMAKE_CXX_ARCHIVE_FINISH   true)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -fomit-frame-pointer ${LTO_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fno-omit-frame-pointer")
set(CMAKE_CXX_FLAGS_PROFILE "-O3 -g -fomit-frame-pointer ${LTO_FLAGS}")

set (DISABLE_STF_DOXYGEN ON)

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    SET(NO_STF_LTO 1)
endif()

add_subdirectory(stf_lib)

include_directories(${STF_BASE}/stf-inc)

add_subdirectory(generators)

add_subdirectory(tools)
