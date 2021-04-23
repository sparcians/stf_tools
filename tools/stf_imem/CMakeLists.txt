project(stf_imem)

include(${STF_CMAKE_DIR}/disassembler.cmake)

add_executable(stf_imem stf_imem.cpp)

target_link_libraries(stf_imem ${STF_LINK_LIBS})
