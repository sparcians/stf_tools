add_library(stf_disasm INTERFACE)
target_link_libraries(stf_disasm INTERFACE binutils_wrapper stf_decoder)

if(NOT DISABLE_BINUTILS)
    target_link_libraries(stf_disasm INTERFACE binutils_wrapper)
    target_compile_definitions(stf_disasm INTERFACE ENABLE_BINUTILS_DISASM)
endif()
