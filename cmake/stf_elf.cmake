if(NOT EXISTS ${STF_TOOLS_BASE}/mavis/ELFIO/.git)
    message(FATAL_ERROR "ELFIO hasn't been checked out. Please run git submodule update --init --recursive")
endif()

include_directories(SYSTEM ${STF_TOOLS_BASE}/mavis/ELFIO)
