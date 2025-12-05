if(NOT DISABLE_BINUTILS)
    project(libbinutils_wrapper C CXX)

    find_package(Iconv)

    set(STF_TOOLS_BINUTILS_WRAPPER_DIR ${STF_TOOLS_LIB_DIR}/binutils_wrapper)

    set(BINUTILS_LIBS ${BINUTILS_LIB_DIR}/libopcodes.a ${BINUTILS_LIB_DIR}/libbfd.a ${BINUTILS_LIB_DIR}/libiberty.a)

    include(isa_overrides OPTIONAL)

    # The error message returned from binutils when an X extension is missing a version number
    set(BINUTILS_XEXT_VERSION_ERROR_MESSAGE "x ISA extension `%s' must be set with the versions")

    if(BINUTILS_HAS_LIBINTL)
        set(BINUTILS_LIBS ${BINUTILS_LIBS} ${BINUTILS_LIB_DIR}/libintl.a)
    else()
        find_package(Intl REQUIRED)
        if(NOT Intl_IS_BUILT_IN)
            set(BINUTILS_LIBS ${BINUTILS_LIBS} Intl::Intl)
        endif()
    endif()
    set(BINUTILS_LIBS ${BINUTILS_LIBS} dl z ${Iconv_LIBRARIES})

    include_directories(SYSTEM ${BINUTILS_INCLUDE_DIRS})
    add_library(binutils_wrapper ${STF_TOOLS_BINUTILS_WRAPPER_DIR}/binutils_wrapper.cpp ${STF_TOOLS_BINUTILS_WRAPPER_DIR}/binutils_c_wrapper.c)

    target_compile_definitions(binutils_wrapper PRIVATE BINUTILS_XEXT_VERSION_ERROR_MESSAGE="${BINUTILS_XEXT_VERSION_ERROR_MESSAGE}")
    target_link_libraries(binutils_wrapper ${BINUTILS_LIBS})

    add_custom_target(check_binutils_xext_version_error_message
                      COMMAND ${STF_TOOLS_BINUTILS_WRAPPER_DIR}/scripts/check_binutils_xext_version_error_message.sh ${BINUTILS_XEXT_VERSION_ERROR_MESSAGE}
                      WORKING_DIRECTORY ${BINUTILS_LIB_DIR}
                      DEPENDS ${STF_TOOLS_BINUTILS_WRAPPER_DIR}/scripts/check_binutils_xext_version_error_message.sh
                      VERBATIM)
    add_dependencies(check_binutils_xext_version_error_message binutils)
    add_dependencies(binutils_wrapper binutils check_binutils_xext_version_error_message)
endif()
