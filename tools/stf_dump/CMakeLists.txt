project(stf_dump)

include(${STF_TOOLS_CMAKE_DIR}/disassembler.cmake)

add_executable(stf_dump stf_dump.cpp)

target_link_libraries(stf_dump ${STF_LINK_LIBS})
