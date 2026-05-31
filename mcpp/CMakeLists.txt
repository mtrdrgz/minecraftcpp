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
target_include_directories(nlohmann_json INTERFACE vendor/nlohmann)

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
add_library(volk STATIC vendor/volk/volk.c vendor/volk/volk.h)
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
