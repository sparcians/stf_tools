project(gen_mavis_json_list)

include(${STF_CMAKE_DIR}/mavis.cmake)

add_executable(gen_mavis_json_list gen_mavis_json_list.cpp)

target_link_libraries(gen_mavis_json_list ${EXTRA_LIBS})
target_compile_definitions(gen_mavis_json_list PRIVATE MAVIS_JSON_PATH="${STF_TOOLS_BASE}/mavis/json")

add_custom_target(mavis_json_files.cpp
                  "gen_mavis_json_list"
                  DEPENDS gen_mavis_json_list
                  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                  BYPRODUCTS ${CMAKE_BINARY_DIR}/mavis_json_files.cpp ${CMAKE_BINARY_DIR}/.mavis_json_cache)

add_library(mavis_json_files ${CMAKE_BINARY_DIR}/mavis_json_files.cpp)
