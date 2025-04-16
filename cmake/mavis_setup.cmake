add_subdirectory(${STF_TOOLS_BASE}/mavis)

if(CMAKE_CXX_COMPILER_ID MATCHES GNU)
    set(EXTRA_MAVIS_COMPILER_FLAGS -Wno-template-id-cdtor)
else()
    set(EXTRA_MAVIS_COMPILER_FLAGS )
endif()

target_compile_options(mavis PRIVATE -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-conversion ${EXTRA_MAVIS_COMPILER_FLAGS})
