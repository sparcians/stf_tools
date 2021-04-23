include_guard(DIRECTORY)

if(NOT EXISTS ${STF_TOOLS_BASE}/mavis/.git)
    message(FATAL_ERROR "Mavis hasn't been checked out. Please run git submodule update --init --recursive")
endif()

include_directories(SYSTEM ${STF_TOOLS_BASE}/mavis)
