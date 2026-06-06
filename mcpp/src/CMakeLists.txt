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

# VerticalAnchor / HeightProvider / FloatProvider parity. Pure standard C++;
# `--cases <tsv>` compares against tools/HeightFloatProviderParity.java (the real
# decompiled value/height providers).
add_executable(height_float_provider_parity
    world/level/levelgen/heightproviders/HeightFloatProviderParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(height_float_provider_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
if(WIN32)
    target_link_libraries(height_float_provider_parity PRIVATE bcrypt)
endif()
set_target_properties(height_float_provider_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# HeightmapPlacement / HeightRangePlacement parity (world-dependent modifiers
# against a stub WorldGenLevel). `--cases <tsv>` compares against
# tools/WorldPlacementParity.java (real modifiers + a Proxy WorldGenLevel).
add_executable(world_placement_parity
    world/level/levelgen/placement/WorldPlacementParityTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
)
target_include_directories(world_placement_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(world_placement_parity PRIVATE glm)
if(WIN32)
    target_link_libraries(world_placement_parity PRIVATE bcrypt)
endif()
set_target_properties(world_placement_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# NoiseRouter / RandomState density parity: samples the seed-wired overworld
# router (temperature..final_density) against tools/DensityParity.java (the real
# decompiled RandomState). Validates audit #6 (mapAll / NoiseWiringHelper wiring).
add_executable(density_parity
    world/level/levelgen/DensityParityTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/RandomState.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/Noises.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/TerrainProvider.cpp
    world/level/levelgen/NoiseSettings.cpp
    world/level/levelgen/NoiseGeneratorSettings.cpp
    world/level/levelgen/NoiseRouterData.cpp
)
target_include_directories(density_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(density_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(density_parity PRIVATE bcrypt)
endif()
set_target_properties(density_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Overworld Climate.Sampler + MultiNoiseBiomeSource preset parity. This consumes
# tools/ClimateBiomeParity.java output and validates quantized climate targets
# plus selected biome ids after the NoiseRouter has been seed-wired.
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

# Base terrain column parity before surface/carvers/decoration. Compares the
# C++ fillFromNoise result against Java NoiseBasedChunkGenerator.getBaseColumn().
add_executable(base_terrain_column_parity
    world/level/levelgen/BaseTerrainColumnParityTest.cpp
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
target_include_directories(base_terrain_column_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(base_terrain_column_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(base_terrain_column_parity PRIVATE bcrypt)
endif()
set_target_properties(base_terrain_column_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Surface terrain column parity after buildSurface and before
# carvers/structures/features/decoration.
add_executable(surface_terrain_column_parity
    world/level/levelgen/SurfaceTerrainColumnParityTest.cpp
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
target_include_directories(surface_terrain_column_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(surface_terrain_column_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(surface_terrain_column_parity PRIVATE bcrypt)
endif()
set_target_properties(surface_terrain_column_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Carved terrain column parity after Java buildSurface+applyCarvers and before
# structures/features/decoration. This is expected to fail until the C++ carver
# port is wired.
add_executable(carved_terrain_column_parity
    world/level/levelgen/CarvedTerrainColumnParityTest.cpp
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
target_include_directories(carved_terrain_column_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(carved_terrain_column_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(carved_terrain_column_parity PRIVATE bcrypt)
endif()
set_target_properties(carved_terrain_column_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# BlockStateProvider parity (Simple / Weighted / NoiseThreshold). Also covers
# raw NormalNoise.getValue (the NOISE cases). `--cases <tsv>` compares against
# tools/BlockStateProviderParity.java (the real decompiled providers).
add_executable(block_state_provider_parity
    world/level/levelgen/feature/stateproviders/BlockStateProviderParityTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
)
target_include_directories(block_state_provider_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(block_state_provider_parity PRIVATE glm)
if(WIN32)
    target_link_libraries(block_state_provider_parity PRIVATE bcrypt)
endif()
set_target_properties(block_state_provider_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# End-to-end vegetation generation demo (patch_grass_plain on a flat test world).
# Ties together the verified pieces via PlacedFeature.place; needs the block-tag
# data dir as argv. Links the noise stack (BIOME_INFO_NOISE) + block tags.
add_executable(vegetation_demo
    world/level/levelgen/feature/VegetationDemoTest.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/block/BlockTags.cpp
)
target_include_directories(vegetation_demo PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(vegetation_demo PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(vegetation_demo PRIVATE bcrypt)
endif()
set_target_properties(vegetation_demo PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Faithful applyBiomeDecoration() driver: real decoration RNG + GenerationStep
# ordering + per-biome feature lists + PlacedFeature pipeline (vs the old heuristic).
add_executable(biome_decorator_test
    world/level/levelgen/feature/BiomeDecoratorTest.cpp
    world/level/levelgen/feature/BiomeDecorator.cpp
    world/level/levelgen/feature/BiomeFeatures.cpp
    world/level/levelgen/feature/TreeGen.cpp
    world/level/chunk/LevelChunk.cpp
    world/level/chunk/PalettedContainer.cpp
    world/level/block/Blocks.cpp
    world/level/block/BlockTags.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
)
target_include_directories(biome_decorator_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(biome_decorator_test PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(biome_decorator_test PRIVATE bcrypt)
endif()
set_target_properties(biome_decorator_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Standalone resource smoke: the exe-embedded MCAS pack must carry enough
# data/minecraft JSON for worldgen decoration without a sidecar 26.1.2 folder.
add_executable(embedded_worldgen_assets_test
    world/level/levelgen/feature/EmbeddedWorldgenAssetsTest.cpp
    assets/AssetPack.cpp
    assets/AssetManager.cpp
    assets/assets.rc
    world/level/block/BlockTags.cpp
    world/level/levelgen/feature/BiomeFeatures.cpp
)
target_include_directories(embedded_worldgen_assets_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(embedded_worldgen_assets_test PRIVATE nlohmann_json)
set_target_properties(embedded_worldgen_assets_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# BlockTags resolver + VegetationBlock canSurvive parity. Pure std C++ +
# nlohmann/json. `--dump <dir>` is cross-checked against tools/block_tags_reference.py.
add_executable(block_tags_parity
    world/level/block/BlockTagsParityTest.cpp
    world/level/block/BlockTags.cpp
)
target_include_directories(block_tags_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/block ${CMAKE_SOURCE_DIR}/vendor)
target_link_libraries(block_tags_parity PRIVATE nlohmann_json)
set_target_properties(block_tags_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Biome registry loader parity (mc::biome::BiomeRegistry). Loads all 65 biomes
# from data/minecraft/worldgen/biome/*.json (pass the dir as arg; defaults to
# 26.1.2/data/...). `--dump` emits a normalised view cross-checked against
# tools/biome_registry_reference.py. Pure standard C++ + nlohmann/json.
add_executable(biome_registry_parity
    world/level/biome/BiomeRegistryParityTest.cpp
    world/level/biome/BiomeRegistry.cpp
)
target_include_directories(biome_registry_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/biome ${CMAKE_SOURCE_DIR}/vendor)
target_link_libraries(biome_registry_parity PRIVATE nlohmann_json)
set_target_properties(biome_registry_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Structure generation check (spaced structures + cross-chunk writer). Drives
# generateStructures over a synthetic flat-stone world; needs the real block
# registry via MCPP_BLOCK_STATES=src/assets/block_states.json.
add_executable(structure_gen_test
    world/level/levelgen/structure/StructureGenTest.cpp
    world/level/levelgen/structure/StructureGen.cpp
    world/level/levelgen/RandomSource.cpp
    world/level/block/Blocks.cpp
)
target_include_directories(structure_gen_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/vendor)
target_link_libraries(structure_gen_test PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(structure_gen_test PRIVATE bcrypt)
endif()
set_target_properties(structure_gen_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
