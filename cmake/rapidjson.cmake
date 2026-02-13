find_package (RapidJSON REQUIRED)

# Rapidjson didn't have a CMake target defined until recently
if(NOT TARGET rapidjson)
    add_library(rapidjson INTERFACE)
    target_include_directories(rapidjson SYSTEM INTERFACE ${RapidJSON_INCLUDE_DIR})
endif()
