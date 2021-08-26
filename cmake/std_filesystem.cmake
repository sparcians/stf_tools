include_guard(DIRECTORY)

set(FILESYSTEM_CHECK_FLAGS ${CMAKE_CXX_FLAGS})
if(CMAKE_CXX_COMPILER_ID MATCHES "AppleClang")
    set(FILESYSTEM_CHECK_FLAGS "${FILESYSTEM_CHECK_FLAGS} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()

execute_process(COMMAND bash "-c" "${CMAKE_CXX_COMPILER} ${FILESYSTEM_CHECK_FLAGS} -E ${STF_TOOLS_BASE}/include/filesystem.hpp | grep \"namespace fs\" | grep -qv experimental"
                RESULT_VARIABLE FILESYSTEM_EXPERIMENTAL)

if (FILESYSTEM_EXPERIMENTAL EQUAL 1)
    SET(EXTRA_LIBS ${EXTRA_LIBS} stdc++fs)
endif()

