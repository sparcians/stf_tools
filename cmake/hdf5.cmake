set(HDF5_USE_STATIC_LIBRARIES 0)

find_package (HDF5 1.10 REQUIRED COMPONENTS CXX)

add_library(stf_hdf5 INTERFACE)
target_include_directories (stf_hdf5 INTERFACE SYSTEM ${HDF5_C_INCLUDE_DIRS} ${HDF5_CXX_INCLUDE_DIRS})
target_link_libraries(stf_hdf5 INTERFACE ${HDF5_CXX_LIBRARIES})

if(HDF5_IS_PARALLEL)
    find_package(MPI REQUIRED COMPONENTS CXX)
    target_include_directories (stf_hdf5 INTERFACE SYSTEM ${MPI_CXX_INCLUDE_DIRS})
    target_link_libraries(stf_hdf5 INTERFACE MPI::MPI_CXX)
    message (STATUS "Using MPI ${MPI_CXX_VERSION}")
endif()

if(STATIC_BUILD)
    target_link_libraries(stf_hdf5 INTERFACE aec)
endif()
