add_executable(asset_packer main.cpp)
target_link_libraries(asset_packer PRIVATE nlohmann_json)
set_target_properties(asset_packer PROPERTIES
    CXX_STANDARD 23
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools"
)
