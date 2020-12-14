if(_ENABLE_BINUTILS_DISASM)
    add_compile_definitions(ENABLE_BINUTILS_DISASM)
    include_directories(${RISCV_INCLUDE_DIRS})
    link_directories(${RISCV_LINK_DIRS})
    set (STF_LINK_LIBS ${STF_LINK_LIBS} opcodes bfd iberty intl dl z ${Iconv_LIBRARIES})
endif()

include(${STF_TOOL_DIR}/mavis.cmake)
