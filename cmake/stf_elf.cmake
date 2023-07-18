if(NOT EXISTS ${STF_TOOLS_BASE}/ELFIO/.git)
    message(FATAL_ERROR "ELFIO hasn't been checked out. Please run git submodule update --init --recursive")
endif()

include_directories(SYSTEM ${STF_TOOLS_BASE}/ELFIO)

ExternalProject_Get_Property(libdwarf install_dir)

include_directories(${install_dir}/include/libdwarf-0)
set(STF_LINK_LIBS ${STF_LINK_LIBS} ${install_dir}/lib/libdwarf.a)
