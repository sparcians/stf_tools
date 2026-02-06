project(stf_branch_hdf5)

add_executable(stf_branch_hdf5 stf_branch_hdf5.cpp)
target_link_libraries(stf_branch_hdf5 stf_hdf5 stf_decoder stf_tools_libs) 
