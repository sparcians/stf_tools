add_subdirectory(${STF_TOOLS_BASE}/mavis)

target_compile_options(mavis PRIVATE -Wno-shorten-64-to-32 -Wno-sign-conversion -Wno-conversion)
