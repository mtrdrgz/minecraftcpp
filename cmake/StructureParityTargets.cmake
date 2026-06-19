# Structure-specific parity/smoke gates that are small enough to keep outside the
# main src/CMakeLists.txt while their runtime wiring is still being staged.

add_executable(nether_fossil_pieces_parity
    src/world/level/levelgen/structure/structures/NetherFossilPiecesParityTest.cpp
    src/world/level/levelgen/RandomSource.cpp
)
target_include_directories(nether_fossil_pieces_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
if(WIN32)
    target_link_libraries(nether_fossil_pieces_parity PRIVATE bcrypt)
endif()
set_target_properties(nether_fossil_pieces_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
