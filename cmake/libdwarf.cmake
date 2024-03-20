include_guard(GLOBAL)

include(ExternalProject)

set(LIBDWARF_CFLAGS ${CMAKE_C_FLAGS})
set(LIBDWARF_CXXFLAGS ${CMAKE_CXX_FLAGS})
set(LIBDWARF_LDFLAGS ${CMAKE_EXE_LINKER_FLAGS})

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(LIBDWARF_CFLAGS "${LIBDWARF_CFLAGS} -O0 -g")
    set(LIBDWARF_CXXFLAGS "${LIBDWARF_CXXFLAGS} -O0 -g")
    set(LIBDWARF_LDFLAGS "${LIBDWARF_LDFLAGS} -Wl,-O0")
endif()

# Set tag
set(LIBDWARF_VERSION 0.6.0)

ExternalProject_Add(
    libdwarf_build
    URL https://github.com/davea42/libdwarf-code/releases/download/v${LIBDWARF_VERSION}/libdwarf-${LIBDWARF_VERSION}.tar.xz
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${LIBDWARF_CFLAGS} CPPFLAGS=${LIBDWARF_CFLAGS} CXXFLAGS=${LIBDWARF_CXXFLAGS} LDFLAGS=${LIBDWARF_LDFLAGS} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND $(MAKE)
    INSTALL_COMMAND $(MAKE) install
    UPDATE_DISCONNECTED true
)

ExternalProject_Get_Property(libdwarf_build SOURCE_DIR)
ExternalProject_Get_Property(libdwarf_build INSTALL_DIR)

set(LIBDWARF_INCLUDE_DIRS ${INSTALL_DIR}/include)
set(LIBDWARF_LIB_DIR ${INSTALL_DIR}/lib)

add_library(libdwarf STATIC IMPORTED GLOBAL)
set_target_properties(libdwarf PROPERTIES IMPORTED_LOCATION ${INSTALL_DIR}/lib/libdwarf.a)
add_dependencies(libdwarf libdwarf_build)

unset(SOURCE_DIR)
unset(INSTALL_DIR)
