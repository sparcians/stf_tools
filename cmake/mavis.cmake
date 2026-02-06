set(MAVIS_DIRECTORY ${STF_TOOLS_BASE}/mavis)

if(NOT EXISTS ${MAVIS_DIRECTORY}/.git)
    message(FATAL_ERROR "Mavis hasn't been checked out. Please run git submodule update --init --recursive")
endif()

add_subdirectory(${MAVIS_DIRECTORY} SYSTEM)

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    set(EXTRA_MAVIS_COMPILER_FLAGS -Wno-template-id-cdtor)
else()
    set(EXTRA_MAVIS_COMPILER_FLAGS )
endif()

target_compile_options(mavis PRIVATE -Wno-pedantic -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-conversion ${EXTRA_MAVIS_COMPILER_FLAGS})

install(DIRECTORY ${MAVIS_DIRECTORY}/json DESTINATION ${STF_INSTALL_DIR}/../share/stf_tools/mavis)
