set(STF_TOOLS_LIB_DIR ${STF_TOOLS_BASE}/lib)

if(NOT DISABLE_BINUTILS)
    add_subdirectory(binutils_wrapper)
endif()
