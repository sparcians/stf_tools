include_guard(DIRECTORY)

if (NOT RISCV_TOOLCHAIN)
    if(NOT RISCV_INSTALL_DIR)
        set (RISCV_INSTALL_DIR /opt/riscv)
    endif()

    if(EXISTS ${RISCV_INSTALL_DIR}/lib64)
        set(RISCV_LIB_DIR ${RISCV_INSTALL_DIR}/lib64)
    elseif(EXISTS ${RISCV_INSTALL_DIR}/lib)
        set(RISCV_LIB_DIR ${RISCV_INSTALL_DIR}/lib)
    endif()

    if(RISCV_LIB_DIR)
        if(EXISTS ${RISCV_LIB_DIR} AND EXISTS ${RISCV_INSTALL_DIR}/include)
            set (RISCV_LINK_DIRS ${RISCV_LINK_DIRS} ${RISCV_LIB_DIR})
            set (RISCV_INCLUDE_DIRS ${RISCV_INCLUDE_DIRS} ${RISCV_INSTALL_DIR}/include)
            set (_ENABLE_BINUTILS_DISASM 1)
        endif()
    endif()
else()
    message ("-- Using RISCV_TOOLCHAIN: ${RISCV_TOOLCHAIN}")
    if(EXISTS ${RISCV_TOOLCHAIN}/build-binutils-linux)
        set (RISCV_LINK_DIRS ${RISCV_LINK_DIRS}
          ${RISCV_TOOLCHAIN}/build-binutils-linux/opcodes
          ${RISCV_TOOLCHAIN}/build-binutils-linux/bfd/
          ${RISCV_TOOLCHAIN}/build-binutils-linux/libiberty
          ${RISCV_TOOLCHAIN}/build-binutils-linux/intl)

        set (RISCV_INCLUDE_DIRS ${RISCV_INCLUDE_DIRS}
          ${RISCV_TOOLCHAIN}/riscv-binutils/opcodes
          ${RISCV_TOOLCHAIN}/riscv-binutils/include
          ${RISCV_TOOLCHAIN}/build-binutils-linux/bfd)

        set (_ENABLE_BINUTILS_DISASM 1)
    elseif(EXISTS ${RISCV_TOOLCHAIN}/build-gdb-newlib)
        message("Couldn't find RISCV Linux toolchain. Falling back to newlib")
        set (RISCV_LINK_DIRS ${RISCV_LINK_DIRS}
          ${RISCV_TOOLCHAIN}/build-gdb-newlib/opcodes
          ${RISCV_TOOLCHAIN}/build-gdb-newlib/bfd/
          ${RISCV_TOOLCHAIN}/build-gdb-newlib/libiberty
          ${RISCV_TOOLCHAIN}/build-gdb-newlib/intl)

        set (RISCV_INCLUDE_DIRS ${RISCV_INCLUDE_DIRS}
          ${RISCV_TOOLCHAIN}/riscv-gdb/opcodes
          ${RISCV_TOOLCHAIN}/riscv-gdb/include
          ${RISCV_TOOLCHAIN}/build-gdb-newlib/bfd)

        set (_ENABLE_BINUTILS_DISASM 1)
    endif()
endif()

if(_ENABLE_BINUTILS_DISASM)
    find_package(Iconv)
    message ("-- Found binutils for disassembly")
else()
    message ("-- Could not find binutils. Only Mavis will be available for disassembly")
endif()

