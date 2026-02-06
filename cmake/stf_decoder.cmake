include(mavis_global_path OPTIONAL)

add_library(stf_decoder INTERFACE)
target_link_libraries(stf_decoder INTERFACE mavis)
