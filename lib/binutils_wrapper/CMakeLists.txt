if(NOT DISABLE_BINUTILS)
    project(libbinutils_wrapper)

    find_package(Iconv)

    set(STF_TOOLS_BINUTILS_WRAPPER_DIR ${STF_TOOLS_LIB_DIR}/binutils_wrapper)

    set(BINUTILS_LIBS ${BINUTILS_LIB_DIR}/libopcodes.a ${BINUTILS_LIB_DIR}/libbfd.a ${BINUTILS_LIB_DIR}/libiberty.a)
    if(BINUTILS_HAS_LIBINTL)
        set(BINUTILS_LIBS ${BINUTILS_LIBS} ${BINUTILS_LIB_DIR}/libintl.a)
    endif()
    set(BINUTILS_LIBS ${BINUTILS_LIBS} dl z ${Iconv_LIBRARIES})

    include_directories(${BINUTILS_INCLUDE_DIRS})
    add_library(binutils_wrapper ${STF_TOOLS_BINUTILS_WRAPPER_DIR}/binutils_wrapper.cpp)

    target_link_libraries(binutils_wrapper ${BINUTILS_LIBS})

    add_dependencies(binutils_wrapper binutils)
endif()
