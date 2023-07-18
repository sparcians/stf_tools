include_guard(GLOBAL)

include(ExternalProject)

set(LIBDWARF_CFLAGS ${CMAKE_C_FLAGS})
set(LIBDWARF_CPPFLAGS ${CMAKE_CPP_FLAGS})
set(LIBDWARF_CXXFLAGS ${CMAKE_CXX_FLAGS})

if (CMAKE_BUILD_TYPE MATCHES "^[Dd]ebug")
    set(LIBDWARF_CFLAGS "${LIBDWARF_CFLAGS} -O0 -g")
    set(LIBDWARF_CPPFLAGS "${LIBDWARF_CPPFLAGS} -O0 -g")
    set(LIBDWARF_CXXFLAGS "${LIBDWARF_CXXFLAGS} -O0 -g")
endif()

# Set tag
set(LIBDWARF_VERSION 0.6.0)

ExternalProject_Add(
    libdwarf
    URL https://github.com/davea42/libdwarf-code/releases/download/v${LIBDWARF_VERSION}/libdwarf-${LIBDWARF_VERSION}.tar.xz
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env CFLAGS=${LIBDWARF_CFLAGS} CPPFLAGS=${LIBDWARF_CPPFLAGS} CXXFLAGS=${LIBDWARF_CXXFLAGS} <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    UPDATE_DISCONNECTED true
)

ExternalProject_Get_Property(libdwarf SOURCE_DIR)
ExternalProject_Get_Property(libdwarf INSTALL_DIR)

set(LIBDWARF_INCLUDE_DIRS ${INSTALL_DIR}/include)
set(LIBDWARF_LIB_DIR ${INSTALL_DIR}/lib)

unset(SOURCE_DIR)
unset(INSTALL_DIR)
