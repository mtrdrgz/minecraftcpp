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
    # The certified whole-chunk decoration machinery (library mode: the engine
    # target defines MCPP_DECORATE_NO_MAIN so the parity main is compiled out;
    # the full_chunk_decorate_parity gate target keeps it and stays the truth).
    world/level/levelgen/feature/FullChunkDecorateParityTest.cpp
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
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/items.json"
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
    # Compile the certified decoration TU as a library (no parity main) — only
    # this one TU checks the define; the parity executable target does NOT set it.
    MCPP_DECORATE_NO_MAIN
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

# Structure PLACEMENT parity vs the real RandomSpreadStructurePlacement +
# StructurePlacement frequency reducers (which chunks are structure chunks per
# seed/set). Ground truth: tools/StructurePlacementParity.java. Reads the real
# structure_set/*.json at runtime from 26.1.2/data.
add_executable(structure_placement_parity
    world/level/levelgen/structure/placement/StructurePlacementParityTest.cpp
    world/level/levelgen/structure/placement/StructurePlacement.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(structure_placement_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(structure_placement_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(structure_placement_parity PRIVATE bcrypt)
endif()
set_target_properties(structure_placement_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Structure TRANSFORM parity vs the real Rotation/Mirror/BoundingBox +
# StructureTemplate static transforms (the pure math under all template/jigsaw
# placement). Header-only; ground truth: tools/StructureTransformsParity.java.
add_executable(structure_transform_parity
    world/level/levelgen/structure/templatesystem/StructureTransformsParityTest.cpp
)
target_include_directories(structure_transform_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(structure_transform_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# CuboidRotation parity vs net.minecraft...cuboid.CuboidRotation (block-model element
# rotation transform + rescale; single-axis + EulerXYZ). Builds on certified JOML
# (rotation/rotationZYX/scale/transformDirection). Ground truth: tools/CuboidRotationParity.java.
add_executable(cuboid_rotation_parity
    render/model/CuboidRotationParityTest.cpp
)
target_include_directories(cuboid_rotation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cuboid_rotation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# FaceBakery geometry-helper parity vs net.minecraft...cuboid.FaceBakery (private
# rotateVertexBy/findClosestDirection/calculateFacing/corner<->center). Block-model
# bake geometry on the certified JOML. Ground truth: tools/FaceBakeryParity.java.
add_executable(face_bakery_parity
    render/model/FaceBakeryParityTest.cpp
)
target_include_directories(face_bakery_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(face_bakery_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# JOML math parity (deterministic subset: Matrix4f rotation(quat)/mul/translate/
# scale/invertAffine/transform*, Vector3f ops, Matrix3f, GeometryUtils.normal) vs
# the real org.joml. Bit-exact via fma + exact-float quaternions (no sin/cos).
# Verifies render/model/Joml.h. Ground truth: tools/JomlMathParity.java.
add_executable(joml_math_parity
    render/model/JomlMathParityTest.cpp
)
target_include_directories(joml_math_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(joml_math_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Vec3 (double-precision movement/physics/raytrace vector) parity vs the real
# net.minecraft.world.phys.Vec3 — bit-exact (Mth-table rotations + correctly-rounded
# sqrt; rotation() atan2/asin tracked separately). Ground truth: tools/Vec3Parity.java.
add_executable(vec3_parity
    world/phys/Vec3ParityTest.cpp
)
target_include_directories(vec3_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec3_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# OctahedralGroup parity vs com.mojang.math.OctahedralGroup + SymmetricGroup3 (the
# 48-element symmetry group behind Rotation/Mirror.rotation() + block model baking).
# Verifies the existing render/model/McMath groundwork. Ground truth:
# tools/OctahedralGroupParity.java.
add_executable(octahedral_parity
    render/model/OctahedralGroupParityTest.cpp
    render/model/McMath.cpp
)
target_include_directories(octahedral_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(octahedral_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ARGB color math parity vs the real net.minecraft.util.ARGB (packing/blend/scale/
# sRGB<->linear/setBrightness). Rendering+GUI color primitives, bit-exact (sRGB LUTs
# embedded from the real class). Ground truth: tools/ARGBParity.java.
add_executable(argb_parity
    util/ARGBParityTest.cpp
)
target_include_directories(argb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(argb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# SimpleBitStorage parity vs the real net.minecraft.util.SimpleBitStorage (packed
# chunk/heightmap long-array, non-spanning, magic-division cellIndex). The disk +
# network wire layout. Ground truth: tools/SimpleBitStorageParity.java.
add_executable(bit_storage_parity
    util/SimpleBitStorageParityTest.cpp
)
target_include_directories(bit_storage_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(bit_storage_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Position long-packing codecs parity vs the real BlockPos/ChunkPos/SectionPos
# (asLong/getX/getY/getZ/offset, blockToSectionCoord, sectionRelative*). The basis
# of chunk/region serialization + long-keyed maps. Ground truth: tools/PosCodecParity.java.
add_executable(pos_codec_parity
    core/PosCodecParityTest.cpp
)
target_include_directories(pos_codec_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(pos_codec_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Mth (engine-wide math utility) parity vs the real net.minecraft.util.Mth — pure
# functions, bit-exact (sin/cos table, atan2 LUT, fastInvSqrt/CubeRoot, hsv,
# murmur, wrapDegrees, lerp/map families, ...). Ground truth: tools/MthParity.java.
add_executable(mth_parity
    world/level/levelgen/MthParityTest.cpp
)
target_include_directories(mth_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mth_parity PROPERTIES
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
# Regenerates assets/block_states.json from the registry ground truth (dev tool).
add_executable(block_states_regen
    world/level/block/BlockStatesRegen.cpp
)
target_include_directories(block_states_regen PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(block_states_regen PRIVATE nlohmann_json)
set_target_properties(block_states_regen PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Block-state REGISTRY parity vs the real Block.BLOCK_STATE_REGISTRY (id order,
# names, trusted flags). Ground truth: tools/BlockStateRegistryParity.java.
add_executable(block_state_registry_parity
    world/level/block/BlockStateRegistryParityTest.cpp
)
target_include_directories(block_state_registry_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(block_state_registry_parity PRIVATE nlohmann_json)
set_target_properties(block_state_registry_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Item REGISTRY parity vs the real BuiltInRegistries.ITEM (id order, names,
# stack/damage, BlockItem linkage, food). Ground truth: tools/ItemRegistryParity.java.
add_executable(item_registry_parity
    world/item/ItemRegistryParityTest.cpp
)
target_include_directories(item_registry_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(item_registry_parity PRIVATE nlohmann_json)
set_target_properties(item_registry_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

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

# Voxel-shape + collision parity vs the real net.minecraft.world.phys.shapes.*
# (VoxelShape/Shapes/BitSetDiscreteVoxelShape/IndexMerger family/SliceShape +
# AABB iterable clip). Ground truth: tools/VoxelShapesParity.java — emits a
# shape-construction program + observations as raw IEEE-754 bit hex; this gate
# replays the program and compares BIT-exactly.
add_executable(voxel_shapes_parity
    world/phys/VoxelShapesParityTest.cpp
    world/phys/AABB.cpp
    world/phys/shapes/DiscreteVoxelShape.cpp
    world/phys/shapes/VoxelShape.cpp
    world/phys/shapes/Shapes.cpp
)
target_include_directories(voxel_shapes_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(voxel_shapes_parity PRIVATE glm)
set_target_properties(voxel_shapes_parity PROPERTIES
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

# Embedded worldgen assets gate: proves assets.bin carries the worldgen biome
# JSON + block tags and that BlockTags/BiomeFeatures::loadFromJsonEntries parse
# them identically to the disk loaders (65 biomes / 244 block tags). Links the
# same assets.rc resource the engine embeds, so it checks the REAL pack.
add_executable(embedded_worldgen_assets
    world/level/levelgen/feature/EmbeddedWorldgenAssetsTest.cpp
    world/level/levelgen/feature/BiomeFeatures.cpp
    world/level/block/BlockTags.cpp
    assets/AssetManager.cpp
    assets/AssetPack.cpp
    assets/assets.rc
)
target_include_directories(embedded_worldgen_assets PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(embedded_worldgen_assets PRIVATE nlohmann_json)
set_target_properties(embedded_worldgen_assets PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)
