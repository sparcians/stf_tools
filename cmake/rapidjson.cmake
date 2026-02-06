find_package (RapidJSON REQUIRED)

add_library(rapidjson INTERFACE)
target_include_directories(rapidjson SYSTEM INTERFACE ${RapidJSON_INCLUDE_DIR})
