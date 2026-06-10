set(MC_SOURCES
    main.cpp
    # Platform
    platform/Window.cpp
    # Assets
    assets/AssetPack.cpp
    assets/AssetManager.cpp
    assets/TextureAtlas.cpp
    assets/assets.rc
    # Core
    core/StbImpl.cpp
    core/ThreadPool.cpp
    # Profiling
    ../../profiling/src/Profiler.cpp

    # NBT
    nbt/Tag.cpp
    nbt/NbtIo.cpp
    # World — Block / Chunk
    world/level/block/Blocks.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/chunk/LevelChunk.cpp
    # World — Levelgen
    world/level/levelgen/Aquifer.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/OreVeinifier.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseRouterData.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/SurfaceRules.cpp
    world/level/levelgen/SurfaceSystem.cpp
    world/level/levelgen/SurfaceRuleData.cpp
    world/level/levelgen/carver/WorldCarver.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/BiomeManager.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
    world/level/levelgen/NoiseBasedChunkGenerator.cpp
    # World — Levelgen / feature decoration
    world/level/block/BlockTags.cpp
    world/level/levelgen/feature/BiomeFeatures.cpp
    world/level/levelgen/feature/BiomeDecorator.cpp
    world/level/levelgen/feature/TreeGen.cpp
    world/level/levelgen/feature/OreGen.cpp
    world/level/levelgen/structure/StructureGen.cpp
    # World — Item
    world/item/Item.cpp
    world/item/Items.cpp
    # World — Inventory
    world/inventory/SimpleContainer.cpp
    world/inventory/Slot.cpp
    # World — Entity
    world/entity/Entity.cpp
    world/entity/Entities.cpp
    world/entity/Mob.cpp
    world/entity/player/Player.cpp
    world/entity/ai/Goal.cpp
    world/entity/ai/GoalSelector.cpp
    world/entity/ai/RandomStrollGoal.cpp
    # World — Physics
    world/phys/AABB.cpp
    # Network
    network/PacketBuffer.cpp
    network/Connection.cpp
    network/protocol/game/PlayPackets.cpp
    # Client
    client/Minecraft.cpp
    client/MinecraftFastStart.cpp
    client/KeyMapping.cpp
    client/player/LocalPlayer.cpp
    gui/Gui.cpp
    gui/components/Button.cpp
    gui/components/OptionWidgets.cpp
    gui/screens/PauseScreen.cpp
    gui/screens/TitleScreen.cpp
    gui/screens/options/OptionsScreen.cpp
    # Render — Common
    render/RenderBackend.cpp
    # Render — OpenGL
    render/opengl/DeviceGL.cpp
    render/opengl/CommandListGL.cpp
    # Render — Vulkan
    render/vulkan/DeviceVK.cpp
    render/vulkan/SwapchainVK.cpp
    render/vulkan/BufferVK.cpp
    render/vulkan/TextureVK.cpp
    render/vulkan/PipelineVK.cpp
    render/vulkan/CommandListVK.cpp
    # Render — DirectX 12
    render/dx12/DeviceDX12.cpp
    render/dx12/SwapchainDX12.cpp
    render/dx12/BufferDX12.cpp
    render/dx12/TextureDX12.cpp
    render/dx12/PipelineDX12.cpp
    render/dx12/CommandListDX12.cpp
    # Render — World
    render/entity/EntityRenderDispatcher.cpp
    render/gui/GuiGraphics.cpp
    render/gui/Font.cpp
    render/gui/PanoramaRenderer.cpp
    render/level/ChunkMesh.cpp
    render/level/LevelRenderer.cpp
    # Audio
    audio/OggDecoder.cpp
    audio/SoundEngine.cpp
    audio/SoundManager.cpp
    ../vendor/stb/stb_vorbis.c
)

set(MC_ASSET_RESOURCE_DEPS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/assets.bin"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/block_atlas.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/block_atlas.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/block_states.json"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/ascii.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_logo.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_button.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_button_hl.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_dirt.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_edition.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/splashes.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_lang.png"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/gui_access.png"
)
set_source_files_properties(assets/assets.rc PROPERTIES
    OBJECT_DEPENDS "${MC_ASSET_RESOURCE_DEPS}"
)

add_executable(mcpp WIN32 ${MC_SOURCES})

if(MSVC)
    target_link_options(mcpp PRIVATE "/ENTRY:mainCRTStartup")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_link_options(mcpp PRIVATE "-Xlinker" "/ENTRY:mainCRTStartup")
endif()


target_include_directories(mcpp PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/render/vulkan
    ${CMAKE_CURRENT_SOURCE_DIR}/render/dx12
    ../vendor/vma
    ../vendor/d3d12ma
    ../../profiling/include
)

target_link_libraries(mcpp PRIVATE
    glm
    nlohmann_json
    stb
    glad
    miniz
    volk
    glslang
    glslang-default-resource-limits
    SPIRV
    MachineIndependent
    GenericCodeGen
    OSDependent
    d3d12ma
)

target_link_libraries(mcpp PRIVATE
    user32 gdi32 kernel32 opengl32
    ws2_32 bcrypt
    ole32
    d3d12 dxgi d3dcompiler
)
# Audio import lib differs by toolchain: llvm-mingw bundles xaudio2_9.lib, while
# the MSVC-ABI Windows SDK ships xaudio2.lib (xaudio2_9.lib does not exist there).
if(MINGW)
    target_link_libraries(mcpp PRIVATE xaudio2_9)
else()
    target_link_libraries(mcpp PRIVATE xaudio2)
endif()

target_compile_definitions(mcpp PRIVATE
    NLOHMANN_JSON_NOEXCEPTION=0
)

set_target_properties(mcpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(debug_test WIN32 debug_main.cpp)
set_target_properties(debug_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(biome_manager_parity
    world/level/levelgen/BiomeManagerParityTest.cpp
    world/level/levelgen/BiomeManager.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
)
target_include_directories(biome_manager_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(biome_manager_parity PRIVATE bcrypt)
set_target_properties(biome_manager_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Overworld biome climate parity (OverworldBiomeBuilder + Climate::RTree).
# Pure standard C++ (no Windows deps), so it also builds/runs on Linux CI.
# Default run does self-contained checks; pass `--cases <file>` to compare
# against TSV ground truth generated by tools/OverworldBiomeParity.java.
add_executable(overworld_biome_parity
    world/level/levelgen/OverworldBiomeParityTest.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
)
target_include_directories(overworld_biome_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
set_target_properties(overworld_biome_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# WorldgenRandom parity (the decoration/feature population RNG). Pure standard
# C++; `--cases <tsv>` compares against ground truth from
# tools/WorldgenRandomParity.java (the real decompiled WorldgenRandom).
add_executable(worldgen_random_parity
    world/level/levelgen/WorldgenRandomParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(worldgen_random_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
if(WIN32)
    target_link_libraries(worldgen_random_parity PRIVATE bcrypt)
endif()
set_target_properties(worldgen_random_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# IntProvider + pure placement-modifier parity. Pure standard C++; `--cases
# <tsv>` compares against ground truth from tools/PlacementParity.java (the real
# decompiled value providers + placement modifiers).
add_executable(placement_parity
    world/level/levelgen/placement/PlacementParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(placement_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(placement_parity PRIVATE glm)
if(WIN32)
    target_link_libraries(placement_parity PRIVATE bcrypt)
endif()
set_target_properties(placement_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Network primitive codec parity vs the real FriendlyByteBuf/VarInt/VarLong.
# Ground truth: tools/FriendlyByteBufParity.java.
add_executable(packet_buffer_parity
    network/PacketBufferParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(packet_buffer_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(packet_buffer_parity PRIVATE miniz)
set_target_properties(packet_buffer_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# NBT tag-codec parity vs the real net.minecraft.nbt: canonical read dumps +
# byte-exact rewrite of real world payloads (level.dat, chunk NBTs) + a synthetic
# all-tag-types / modified-UTF-8 case. Ground truth: tools/NbtParity.java.
# AABB (ray clip / expandTowards / contract / distanceToSqr) parity vs the real
# net.minecraft.world.phys.AABB. Ground truth: tools/AabbParity.java (bit-exact).
add_executable(aabb_parity
    world/phys/AabbParityTest.cpp
    world/phys/AABB.cpp
)
target_include_directories(aabb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(aabb_parity PRIVATE glm)
set_target_properties(aabb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(nbt_parity
    nbt/NbtParityTest.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(nbt_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(nbt_parity PRIVATE miniz)
set_target_properties(nbt_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Full-chunk terrain parity: proves the C++ generator is byte-for-byte identical to
# the real Java generator over EVERY block of a chunk (not sampled columns), through
# fillFromNoise + buildSurface + applyCarvers. Ground truth: tools/FullChunkParity.java.
# Source list is the certified generator set (same as the removed Base/Surface/Carved
# column targets, commit 0bfc8121) plus the new full-chunk test.
add_executable(full_chunk_parity
    world/level/levelgen/FullChunkParityTest.cpp
    world/level/levelgen/NoiseBasedChunkGenerator.cpp
    world/level/levelgen/Aquifer.cpp
    world/level/levelgen/OreVeinifier.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseRouterData.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/BiomeManager.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
    world/level/levelgen/SurfaceSystem.cpp
    world/level/levelgen/SurfaceRules.cpp
    world/level/levelgen/SurfaceRuleData.cpp
    world/level/levelgen/carver/WorldCarver.cpp
    world/level/chunk/LevelChunk.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/block/Blocks.cpp
)
target_include_directories(full_chunk_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(full_chunk_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(full_chunk_parity PRIVATE bcrypt)
endif()
set_target_properties(full_chunk_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Dev scout (NOT a parity gate): per-chunk noise-biome map for a seed/rect via the
# certified climate pipeline. Used to find target biome classes before server runs.
add_executable(biome_scout
    world/level/levelgen/BiomeScoutTool.cpp
    world/level/levelgen/NoiseBasedChunkGenerator.cpp
    world/level/levelgen/Aquifer.cpp
    world/level/levelgen/OreVeinifier.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseRouterData.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/BiomeManager.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
    world/level/levelgen/SurfaceSystem.cpp
    world/level/levelgen/SurfaceRules.cpp
    world/level/levelgen/SurfaceRuleData.cpp
    world/level/levelgen/carver/WorldCarver.cpp
    world/level/chunk/LevelChunk.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/block/Blocks.cpp
)
target_include_directories(biome_scout PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(biome_scout PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(biome_scout PRIVATE bcrypt)
endif()
set_target_properties(biome_scout PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Per-feature decoration parity (restored; dropped by c7abe591). Certifies one
# placed/configured feature 1:1 against the real Java PlacedFeature.placeWithBiomeCheck
# via tools/BiomeDecorationParity.java. Used to certify each decoration family before
# wiring it into the whole-chunk applyBiomeDecoration path.
add_executable(biome_decoration_parity
    world/level/levelgen/feature/BiomeDecorationParityTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/chunk/LevelChunk.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/block/Blocks.cpp
    world/level/block/BlockTags.cpp
)
target_include_directories(biome_decoration_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(biome_decoration_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(biome_decoration_parity PRIVATE bcrypt)
endif()
set_target_properties(biome_decoration_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Whole-chunk DECORATION parity vs the no-structures server (.mca): generates terrain
# for a 5x5, decorates the inner 3x3 (ported features only) into a shared region, and
# compares chunk C vs the server dump. Same generator sources as full_chunk_parity, plus
# the biome feature lists + block tags the decoration loop needs.
add_executable(full_chunk_decorate_parity
    world/level/levelgen/feature/FullChunkDecorateParityTest.cpp
    world/level/levelgen/NoiseBasedChunkGenerator.cpp
    world/level/levelgen/Aquifer.cpp
    world/level/levelgen/OreVeinifier.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseRouterData.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/BiomeManager.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
    world/level/levelgen/SurfaceSystem.cpp
    world/level/levelgen/SurfaceRules.cpp
    world/level/levelgen/SurfaceRuleData.cpp
    world/level/levelgen/carver/WorldCarver.cpp
    world/level/levelgen/feature/BiomeFeatures.cpp
    world/level/chunk/LevelChunk.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/block/Blocks.cpp
    world/level/block/BlockTags.cpp
    nbt/Tag.cpp
    nbt/NbtIo.cpp
)
target_include_directories(full_chunk_decorate_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(full_chunk_decorate_parity PRIVATE glm nlohmann_json miniz)
if(WIN32)
    target_link_libraries(full_chunk_decorate_parity PRIVATE bcrypt)
endif()
set_target_properties(full_chunk_decorate_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    CXX_SCAN_FOR_MODULES OFF
)

# Climate biome parity (restored; dropped by c7abe591, added by a3cbc510): certifies
# Climate.Sampler targets + MultiNoiseBiomeSource.findValue vs tools/ClimateBiomeParity.java.
add_executable(climate_biome_parity
    world/level/levelgen/ClimateBiomeParityTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseRouterData.cpp
    world/level/levelgen/BiomeSource.cpp
    world/level/levelgen/OverworldBiomeBuilder.cpp
)
target_include_directories(climate_biome_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(climate_biome_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(climate_biome_parity PRIVATE bcrypt)
endif()
set_target_properties(climate_biome_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
