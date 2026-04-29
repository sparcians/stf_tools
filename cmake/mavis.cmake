set(MAVIS_DIRECTORY ${STF_TOOLS_BASE}/mavis)

if(NOT EXISTS ${MAVIS_DIRECTORY}/.git)
    message(FATAL_ERROR "Mavis hasn't been checked out. Please run git submodule update --init --recursive")
endif()

add_subdirectory(${MAVIS_DIRECTORY} SYSTEM)

install(DIRECTORY ${MAVIS_DIRECTORY}/json DESTINATION ${STF_INSTALL_DIR}/../share/stf_tools/mavis)
