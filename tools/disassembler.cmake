find_package(Iconv)

if (NOT RISCV_TOOLCHAIN)
    if(NOT RISCV_INSTALL_DIR)
        set (RISCV_INSTALL_DIR /opt/riscv)
    endif()
    if(EXISTS ${RISCV_INSTALL_DIR}/lib64)
        set(RISCV_LIB_DIR ${RISCV_INSTALL_DIR}/lib64)
    elseif(EXISTS ${RISCV_INSTALL_DIR}/lib)
        set(RISCV_LIB_DIR ${RISCV_INSTALL_DIR}/lib)
    else()
        message(FATAL_ERROR "RISCV_TOOLCHAIN not defined and RISCV libraries not installed at ${RISCV_INSTALL_DIR}. Please specify -DRISCV_TOOLCHAIN=/path/to/riscv/")
    endif()
    if(NOT EXISTS ${RISCV_INSTALL_DIR}/include)
      message(FATAL_ERROR "RISCV_TOOLCHAIN not defined and RISCV headers not installed at ${RISCV_INSTALL_DIR}. Please specify -DRISCV_TOOLCHAIN=/path/to/riscv/")
    endif()
    set (RISCV_LINK_DIRS ${RISCV_LINK_DIRS} ${RISCV_LIB_DIR})
    set (RISCV_INCLUDE_DIRS ${RISCV_INCLUDE_DIRS} ${RISCV_INSTALL_DIR}/include)
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
    else()
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
    endif()
endif()

include_directories(${RISCV_INCLUDE_DIRS})
link_directories(${RISCV_LINK_DIRS})
set (STF_LINK_LIBS ${STF_LINK_LIBS} opcodes bfd iberty intl dl z ${Iconv_LIBRARIES})
