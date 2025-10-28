set(MAVIS_DIRECTORY ${STF_TOOLS_BASE}/mavis)

add_subdirectory(${MAVIS_DIRECTORY})

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    set(EXTRA_MAVIS_COMPILER_FLAGS -Wno-template-id-cdtor)
else()
    set(EXTRA_MAVIS_COMPILER_FLAGS )
endif()

target_compile_options(mavis PRIVATE -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-conversion ${EXTRA_MAVIS_COMPILER_FLAGS})

install(DIRECTORY ${MAVIS_DIRECTORY}/json DESTINATION ${STF_INSTALL_DIR}/../share/stf_tools/mavis)
