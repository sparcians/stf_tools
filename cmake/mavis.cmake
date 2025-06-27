include_guard(DIRECTORY)

if(NOT EXISTS ${STF_TOOLS_BASE}/mavis/.git)
    message(FATAL_ERROR "Mavis hasn't been checked out. Please run git submodule update --init --recursive")
endif()

include(${STF_TOOLS_CMAKE_DIR}/mavis_global_path.cmake OPTIONAL)

set(STF_LINK_LIBS ${STF_LINK_LIBS} Boost::json)

include_directories(SYSTEM ${STF_TOOLS_BASE}/mavis)
