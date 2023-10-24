include_guard(GLOBAL)

include(ExternalProject)

if(DISABLE_BINUTILS)
    message("-- Disabling binutils support")
else()
    set(BINUTILS_CFLAGS "${CMAKE_C_FLAGS} -Wno-unknown-warning-option")
    set(BINUTILS_CXXFLAGS "${CMAKE_CXX_FLAGS} -Wno-unknown-warning-option")
    set(BINUTILS_LDFLAGS ${CMAKE_EXE_LINKER_FLAGS})

    if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
        set(BINUTILS_CFLAGS "${BINUTILS_CFLAGS} -O0 -g")
        set(BINUTILS_CXXFLAGS "${BINUTILS_CXXFLAGS} -O0 -g")
        set(BINUTILS_LDFLAGS "${BINUTILS_LDFLAGS} -Wl,-O0")
    endif()

    set(BINUTILS_PATCHES ${STF_TOOLS_PATCHES_DIR}/riscv-binutils-gdb/0001-Add-riscv_get_disassembler_arch-function.patch
                         ${STF_TOOLS_PATCHES_DIR}/riscv-binutils-gdb/0002-Make-it-possible-to-override-opcodes_error_handler.patch
                         ${STF_TOOLS_PATCHES_DIR}/riscv-binutils-gdb/0003-enable-zihintntl.patch
    )

    # Set tag
    set(BINUTILS_TAG binutils-2_41)

    ExternalProject_Add(
        binutils
        GIT_REPOSITORY git://sourceware.org/git/binutils-gdb.git
        GIT_TAG ${BINUTILS_TAG}
        PATCH_COMMAND ${STF_TOOLS_PATCHES_DIR}/apply_patches.sh ${BINUTILS_TAG} ${BINUTILS_PATCHES}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=${BINUTILS_CFLAGS} CPPFLAGS=${BINUTILS_CFLAGS} CXXFLAGS=${BINUTILS_CXXFLAGS} LDFLAGS=${BINUTILS_LDFLAGS} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --disable-gas --disable-etc --disable-libbacktrace --disable-libdecnumber --disable-gnulib --disable-readline --disable-sim --disable-gdbserver --disable-gdbsupport --disable-gprof --disable-gdb --disable-libctf --disable-ld --disable-binutils --target=riscv64-unknown-linux-gnu
        BUILD_COMMAND make all-bfd all-opcodes all-libiberty all-intl
        INSTALL_COMMAND make install-bfd install-opcodes install-libiberty
        UPDATE_DISCONNECTED true
    )

    ExternalProject_Add_Step(binutils
                             install_dirs
                             WORKING_DIRECTORY <INSTALL_DIR>
                             COMMAND mkdir -p lib include
                             DEPENDEES install)

    ExternalProject_Add_Step(binutils
                             build_libintl
                             WORKING_DIRECTORY <BINARY_DIR>/intl
                             COMMAND make libintl.a
                             DEPENDEES build
                             DEPENDERS install)

    ExternalProject_Add_Step(binutils
                             install_libs
                             WORKING_DIRECTORY <BINARY_DIR>
                             COMMAND cp intl/libintl.a opcodes/libopcodes.a libiberty/libiberty.a bfd/.libs/libbfd.a <INSTALL_DIR>/lib/
                             DEPENDEES install_dirs)

    ExternalProject_Add_Step(binutils
                             install_generated_headers
                             WORKING_DIRECTORY <BINARY_DIR>
                             COMMAND cp bfd/bfd.h bfd/config.h <INSTALL_DIR>/include/
                             DEPENDEES install_dirs)

    ExternalProject_Get_Property(binutils SOURCE_DIR)
    ExternalProject_Get_Property(binutils INSTALL_DIR)

    set(BINUTILS_INCLUDE_DIRS ${INSTALL_DIR}/include ${SOURCE_DIR}/include ${SOURCE_DIR}/bfd ${SOURCE_DIR}/opcodes)
    set(BINUTILS_LIB_DIR ${INSTALL_DIR}/lib)

    unset(SOURCE_DIR)
    unset(INSTALL_DIR)
endif()
