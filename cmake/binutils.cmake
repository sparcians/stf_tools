include_guard(GLOBAL)

include(ExternalProject)

include(binutils_internal OPTIONAL)

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

    if(NOT USE_INTERNAL_BINUTILS)
        set(BINUTILS_REPO git://sourceware.org/git/binutils-gdb.git)

        set(BINUTILS_PATCHES
            ${STF_TOOLS_PATCHES_DIR}/riscv-binutils-gdb/0001-Add-riscv_get_disassembler_arch-function.patch
            ${STF_TOOLS_PATCHES_DIR}/riscv-binutils-gdb/0002-Make-it-possible-to-override-opcodes_error_handler.patch
        )

        # Set tag
        set(BINUTILS_TAG binutils-2_42)
    endif()

    message("-- Building binutils from ${BINUTILS_REPO}, tag ${BINUTILS_TAG}")

    set(_BINUTILS_MAKE_TARGETS all-bfd all-opcodes all-libiberty)

    if(BINUTILS_HAS_LIBINTL)
        set(_BINUTILS_MAKE_TARGETS ${_BINUTILS_MAKE_TARGETS} all-intl)
    endif()

    if(STATIC_BUILD)
        set(HOST_CONFIGARGS "--enable-plugin=no --enable-plugins=no")
    else()
        set(HOST_CONFIGARGS )
    endif()

    ExternalProject_Add(
        binutils
        GIT_REPOSITORY ${BINUTILS_REPO}
        GIT_TAG ${BINUTILS_TAG}
        PATCH_COMMAND ${STF_TOOLS_PATCHES_DIR}/apply_patches.sh ${BINUTILS_TAG} ${BINUTILS_PATCHES}
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER} CFLAGS=${BINUTILS_CFLAGS} CPPFLAGS=${BINUTILS_CFLAGS} CXXFLAGS=${BINUTILS_CXXFLAGS} LDFLAGS=${BINUTILS_LDFLAGS} host_configargs=${HOST_CONFIGARGS} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR> --disable-gas --disable-etc --disable-libbacktrace --disable-libdecnumber --disable-gnulib --disable-readline --disable-sim --disable-gdbserver --disable-gdbsupport --disable-gprof --disable-gdb --disable-libctf --disable-ld --disable-binutils --target=riscv64-unknown-linux-gnu
        BUILD_COMMAND $(MAKE) ${_BINUTILS_MAKE_TARGETS}
        INSTALL_COMMAND $(MAKE) install-bfd install-opcodes install-libiberty
        UPDATE_DISCONNECTED true
    )

    ExternalProject_Add_Step(binutils
                             install_dirs
                             WORKING_DIRECTORY <INSTALL_DIR>
                             COMMAND mkdir -p lib include
                             DEPENDEES install)

    set(_BINUTILS_LIBS opcodes/libopcodes.a libiberty/libiberty.a bfd/.libs/libbfd.a)

    if(BINUTILS_HAS_LIBINTL)
        ExternalProject_Add_Step(binutils
                                 build_libintl
                                 WORKING_DIRECTORY <BINARY_DIR>/intl
                                 COMMAND $(MAKE) libintl.a
                                 DEPENDEES build
                                 DEPENDERS install)
        set(_BINUTILS_LIBS ${_BINUTILS_LIBS} intl/libintl.a)
    endif()

    ExternalProject_Add_Step(binutils
                             install_libs
                             WORKING_DIRECTORY <BINARY_DIR>
                             COMMAND cp ${_BINUTILS_LIBS} <INSTALL_DIR>/lib/
                             DEPENDEES install_dirs)

    ExternalProject_Add_Step(binutils
                             install_generated_headers
                             WORKING_DIRECTORY <BINARY_DIR>
                             COMMAND cp bfd/bfd.h bfd/bfdver.h bfd/config.h ${BINUTILS_EXTRA_BFD_HEADERS} <INSTALL_DIR>/include/
                             DEPENDEES install_dirs)

    ExternalProject_Get_Property(binutils SOURCE_DIR)
    ExternalProject_Get_Property(binutils INSTALL_DIR)

    set(BINUTILS_INCLUDE_DIRS ${INSTALL_DIR}/include ${SOURCE_DIR}/include ${SOURCE_DIR}/bfd ${SOURCE_DIR}/opcodes)
    set(BINUTILS_LIB_DIR ${INSTALL_DIR}/lib)

    unset(SOURCE_DIR)
    unset(INSTALL_DIR)
endif()
