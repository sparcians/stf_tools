if(NOT DISABLE_BINUTILS)
    set(STF_LINK_LIBS ${STF_LINK_LIBS} binutils_wrapper)
    add_compile_definitions(ENABLE_BINUTILS_DISASM)
endif()

include(${STF_TOOLS_CMAKE_DIR}/stf_decoder.cmake)
