cmake_minimum_required(VERSION 3.28)
project(mcpp VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(cmake/CompilerFlags.cmake)

# ── Vendor targets ──────────────────────────────────────────────────────────

# GLM (header-only)
add_library(glm INTERFACE)
target_include_directories(glm INTERFACE vendor)
target_compile_definitions(glm INTERFACE
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    GLM_FORCE_LEFT_HANDED
    GLM_ENABLE_EXPERIMENTAL
)

# nlohmann/json (header-only)
add_library(nlohmann_json INTERFACE)
target_include_directories(nlohmann_json INTERFACE vendor)

# stb (header-only, each TU that uses it must define the implementation macro once)
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE vendor/stb)

# GLAD (OpenGL 4.6 core)
add_library(glad STATIC vendor/glad/src/gl.c)
target_include_directories(glad PUBLIC vendor/glad/include)

# miniz (compression)
add_library(miniz STATIC
    vendor/miniz/miniz.c
    vendor/miniz/miniz_tdef.c
    vendor/miniz/miniz_tinfl.c
    vendor/miniz/miniz_zip.c
)
target_include_directories(miniz PUBLIC vendor/miniz)
target_compile_definitions(miniz PRIVATE _CRT_SECURE_NO_WARNINGS)

# VMA (header-only, needs Vulkan - linked later when Vulkan backend is built)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE vendor/vma)

# volk (Vulkan meta-loader)
add_library(volk STATIC vendor/volk/volk.c vendor/volk.h)
target_include_directories(volk PUBLIC vendor/volk vendor)
if(WIN32)
    target_compile_definitions(volk PUBLIC VK_USE_PLATFORM_WIN32_KHR)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(volk PRIVATE dl)
endif()

# glslang (GLSL compiler)
add_subdirectory(vendor/glslang)

# D3D12MA (D3D12 Memory Allocator)
if(WIN32)
    add_library(d3d12ma STATIC
        vendor/d3d12ma/D3D12MemAlloc.cpp
        vendor/d3d12ma/D3D12MemAlloc.h
    )
    target_include_directories(d3d12ma PUBLIC vendor/d3d12ma)
    target_link_libraries(d3d12ma PUBLIC d3d12 dxgi)
endif()

# ── Subdirectories ───────────────────────────────────────────────────────────

add_subdirectory(tools/asset_packer)
add_subdirectory(src)

# ── Runtime asset pack ───────────────────────────────────────────────────────
# Always rebuild this during mcpp builds. CI may create tiny dependency files in
# src/assets, but the shipped executable must embed the real selected pack.
set(MCPP_RUNTIME_ASSET_PACK "${CMAKE_CURRENT_SOURCE_DIR}/src/assets/assets.bin")
set(MCPP_SRC_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/assets")
set(MCPP_EXTERNAL_ASSETS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../assets")
set(MCPP_WORLDGEN_DATA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../26.1.2/data/minecraft")
set(MCPP_PREPARE_RUNTIME_ASSETS "${CMAKE_CURRENT_SOURCE_DIR}/cmake/PrepareRuntimeAssets.cmake")

set(MCPP_ASSET_PACK_COMMAND
    asset_packer
    "${MCPP_EXTERNAL_ASSETS_DIR}"
    "${MCPP_SRC_ASSETS_DIR}"
    "${MCPP_RUNTIME_ASSET_PACK}"
)
if(EXISTS "${MCPP_WORLDGEN_DATA_DIR}")
    list(APPEND MCPP_ASSET_PACK_COMMAND "${MCPP_WORLDGEN_DATA_DIR}")
endif()

add_custom_target(pack_runtime_assets
    COMMAND "${CMAKE_COMMAND}" -P "${MCPP_PREPARE_RUNTIME_ASSETS}"
    COMMAND ${MCPP_ASSET_PACK_COMMAND}
    COMMAND "${CMAKE_COMMAND}" -DASSET_BIN=${MCPP_RUNTIME_ASSET_PACK} -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/VerifyAssetPack.cmake"
    BYPRODUCTS "${MCPP_RUNTIME_ASSET_PACK}"
    DEPENDS asset_packer "${MCPP_PREPARE_RUNTIME_ASSETS}"
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    COMMENT "Packing runtime assets into src/assets/assets.bin"
    VERBATIM
)
add_dependencies(mcpp pack_runtime_assets)
set_source_files_properties(src/assets/assets.rc PROPERTIES
    OBJECT_DEPENDS "${MCPP_RUNTIME_ASSET_PACK}"
)
