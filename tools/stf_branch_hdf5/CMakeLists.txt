project(stf_branch_hdf5)

include(${STF_TOOLS_CMAKE_DIR}/hdf5.cmake)

add_executable(stf_branch_hdf5 stf_branch_hdf5.cpp)

target_link_libraries(stf_branch_hdf5 ${STF_LINK_LIBS})
