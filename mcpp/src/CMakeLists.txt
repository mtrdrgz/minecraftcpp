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

# Entity collision-slide parity vs net.minecraft.world.entity.Entity.collideWithShapes
# (axis-ordered AABB-vs-VoxelShape resolution). On certified Shapes.collide + AABB +
# Vec3. Ground truth: tools/EntityCollisionParity.java.
add_executable(entity_collision_parity
    world/entity/EntityCollisionParityTest.cpp
    world/phys/AABB.cpp
    world/phys/shapes/DiscreteVoxelShape.cpp
    world/phys/shapes/VoxelShape.cpp
    world/phys/shapes/Shapes.cpp
)
target_include_directories(entity_collision_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entity_collision_parity PRIVATE glm)
set_target_properties(entity_collision_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Entity movement-input math parity vs net.minecraft.world.entity.Entity.getInputVector
# (WASD input + yaw -> world delta; core of moveRelative). On certified Vec3 + Mth.
# Ground truth: tools/EntityMovementParity.java.
add_executable(entity_movement_parity
    world/entity/EntityMovementParityTest.cpp
)
target_include_directories(entity_movement_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(entity_movement_parity PROPERTIES
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

# ───────────────────────── Parallel-authored bounded gates (workflow batch) ─────────────────────────

add_executable(vec3i_parity core/Vec3iParityTest.cpp)
target_include_directories(vec3i_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec3i_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(vec2_parity world/phys/Vec2ParityTest.cpp)
target_include_directories(vec2_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec2_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(quart_pos_parity core/QuartPosParityTest.cpp)
target_include_directories(quart_pos_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(quart_pos_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(front_and_top_parity core/FrontAndTopParityTest.cpp)
target_include_directories(front_and_top_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(front_and_top_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(hit_result_parity world/phys/HitResultParityTest.cpp)
target_include_directories(hit_result_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(hit_result_parity PRIVATE glm)
set_target_properties(hit_result_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(smooth_double_parity util/SmoothDoubleParityTest.cpp)
target_include_directories(smooth_double_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(smooth_double_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(brightness_parity util/BrightnessParityTest.cpp)
target_include_directories(brightness_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(brightness_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(math_axis_parity render/model/MathAxisParityTest.cpp)
target_include_directories(math_axis_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(math_axis_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(cubic_spline_parity world/level/levelgen/CubicSplineParityTest.cpp)
target_include_directories(cubic_spline_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cubic_spline_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(transformation_parity render/model/TransformationParityTest.cpp)
target_include_directories(transformation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(transformation_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(direction2d_parity core/Direction2DParityTest.cpp)
target_include_directories(direction2d_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(direction2d_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(chunk_pos_extra_parity world/level/ChunkPosExtraParityTest.cpp)
target_include_directories(chunk_pos_extra_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(chunk_pos_extra_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ───────────────────────── Parallel-authored bounded gates (workflow wave 2) ─────────────────────────

add_executable(entity_dimensions_parity world/entity/EntityDimensionsParityTest.cpp)
target_include_directories(entity_dimensions_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entity_dimensions_parity PRIVATE glm)
set_target_properties(entity_dimensions_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(combat_rules_parity world/damagesource/CombatRulesParityTest.cpp)
target_include_directories(combat_rules_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(combat_rules_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(player_xp_parity world/entity/player/PlayerXpParityTest.cpp)
target_include_directories(player_xp_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(player_xp_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(attribute_math_parity world/entity/ai/attributes/AttributeMathParityTest.cpp)
target_include_directories(attribute_math_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(attribute_math_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(data_layer_parity world/level/chunk/DataLayerParityTest.cpp)
target_include_directories(data_layer_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(data_layer_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(gui_scale_parity client/GuiScaleParityTest.cpp)
target_include_directories(gui_scale_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(gui_scale_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(matrix_util_parity render/model/MatrixUtilParityTest.cpp)
target_include_directories(matrix_util_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(matrix_util_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(pose_stack_parity render/PoseStackParityTest.cpp)
target_include_directories(pose_stack_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(pose_stack_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(weighted_random_parity util/WeightedRandomParityTest.cpp world/level/levelgen/RandomSource.cpp)
target_include_directories(weighted_random_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(weighted_random_parity PRIVATE bcrypt)
endif()
set_target_properties(weighted_random_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(improved_noise_parity
    world/level/levelgen/synth/ImprovedNoiseParityTest.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(improved_noise_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
target_link_libraries(improved_noise_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(improved_noise_parity PRIVATE bcrypt)
endif()
set_target_properties(improved_noise_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(frustum_parity render/FrustumParityTest.cpp)
target_include_directories(frustum_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(frustum_parity PRIVATE glm)
set_target_properties(frustum_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(food_data_parity world/food/FoodDataParityTest.cpp)
target_include_directories(food_data_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(food_data_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(simplex_noise_parity
    world/level/levelgen/synth/SimplexNoiseParityTest.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(simplex_noise_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
target_link_libraries(simplex_noise_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(simplex_noise_parity PRIVATE bcrypt)
endif()
set_target_properties(simplex_noise_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ───────────────────────── Parallel-authored bounded gates (workflow wave 3) ─────────────────────────

set(WAVE3_NOISE_SRC world/level/levelgen/Noise.cpp world/level/levelgen/DensityFunction.cpp world/level/levelgen/CubicSpline.cpp world/level/levelgen/RandomSource.cpp)
add_executable(perlin_noise_parity world/level/levelgen/synth/PerlinNoiseParityTest.cpp ${WAVE3_NOISE_SRC})
add_executable(normal_noise_parity world/level/levelgen/synth/NormalNoiseParityTest.cpp ${WAVE3_NOISE_SRC})
add_executable(blended_noise_parity world/level/levelgen/synth/BlendedNoiseParityTest.cpp ${WAVE3_NOISE_SRC})
add_executable(perlin_simplex_noise_parity world/level/levelgen/synth/PerlinSimplexNoiseParityTest.cpp ${WAVE3_NOISE_SRC})
add_executable(density_fns_parity world/level/levelgen/DensityFnsParityTest.cpp ${WAVE3_NOISE_SRC})
foreach(t perlin_noise_parity normal_noise_parity blended_noise_parity perlin_simplex_noise_parity density_fns_parity)
    target_include_directories(${t} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
    target_link_libraries(${t} PRIVATE glm nlohmann_json)
    if(WIN32)
        target_link_libraries(${t} PRIVATE bcrypt)
    endif()
    set_target_properties(${t} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
endforeach()

add_executable(vec_delta_codec_parity world/entity/VecDeltaCodecParityTest.cpp)
target_include_directories(vec_delta_codec_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec_delta_codec_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(id_bimap_parity util/IdBiMapParityTest.cpp)
target_include_directories(id_bimap_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(id_bimap_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(block_state_props_parity world/level/block/state/properties/BlockStatePropsParityTest.cpp)
target_include_directories(block_state_props_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(block_state_props_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(pathfinder_node_parity world/level/pathfinder/PathfinderNodeParityTest.cpp)
target_include_directories(pathfinder_node_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pathfinder_node_parity PRIVATE glm)
set_target_properties(pathfinder_node_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(light_texture_parity render/LightTextureParityTest.cpp)
target_include_directories(light_texture_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(light_texture_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(quaternionf_math_parity render/model/QuaternionMathParityTest.cpp)
target_include_directories(quaternionf_math_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(quaternionf_math_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

add_executable(explosion_falloff_parity world/level/ExplosionMathParityTest.cpp)
target_include_directories(explosion_falloff_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(explosion_falloff_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== Parallel-authored bounded gates (workflow wave 4) =====

# net.minecraft.world.level.block.state.properties (AttachFace/Half/SlabType/StairsShape/BedPart/ChestType/ComparatorMode/WallSide/DoorHingeSide/PistonType/RedstoneSide/RailShape/DoubleBlockHalf/Tilt)
add_executable(attach_face_props_parity world/level/block/state/properties/BlockStateEnumsParityTest.cpp)
target_include_directories(attach_face_props_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(attach_face_props_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.BiasedToBottomInt
add_executable(biased_to_bottom_int_parity util/valueproviders/BiasedToBottomIntParityTest.cpp world/level/levelgen/RandomSource.cpp)
target_include_directories(biased_to_bottom_int_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(biased_to_bottom_int_parity PRIVATE bcrypt)
endif()
set_target_properties(biased_to_bottom_int_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.BinaryAnimator
add_executable(binary_animator_parity
    util/BinaryAnimatorParityTest.cpp
)
target_include_directories(binary_animator_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(binary_animator_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.pathfinder.BinaryHeap
add_executable(binary_heap_parity world/level/pathfinder/BinaryHeapParityTest.cpp)
target_include_directories(binary_heap_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(binary_heap_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.biome.Biome
add_executable(biome_temperature_parity
    world/level/levelgen/BiomeTemperatureParityTest.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(biome_temperature_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
target_link_libraries(biome_temperature_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(biome_temperature_parity PRIVATE bcrypt)
endif()
set_target_properties(biome_temperature_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.BlockBox
add_executable(block_box_parity core/BlockBoxParityTest.cpp)
target_include_directories(block_box_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(block_box_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.BlockMath
# net.minecraft.core.BlockMath parity: UV-lock face transformation
# (getFaceTransformation + the two VANILLA_UV_TRANSFORM_* Direction->Transformation
# maps + Direction.getApproximateNearest). Header-only on the certified
# render/model/Joml.h subset (Matrix4f rotation/translation/mul/invertAffine/
# transformDirection, Quaternionf.rotateX/rotateY via jsin+cosFromSin,
# MatrixUtil.isIdentity). No registry/world deps. Ground truth: tools/BlockMathParity.java.
add_executable(block_math_parity
    core/BlockMathParityTest.cpp
)
target_include_directories(block_math_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(block_math_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.phys.shapes.BooleanOp
# BooleanOp truth-table parity vs the real net.minecraft.world.phys.shapes.BooleanOp
# (the 16 named two-input boolean operators FALSE..TRUE used by Shapes.join).
# Ground truth: tools/BooleanOpParity.java reflects each BooleanOp lambda field and
# invokes the real apply(boolean,boolean) over the 2x2 truth table; this gate
# dispatches the same op by name and compares the boolean result bit-for-bit.
# The included header (world/phys/shapes/BooleanOp.h) is self-contained (pure
# constexpr lambdas) so the target needs no extra sources and no external libs.
add_executable(boolean_op_parity
    world/level/levelgen/BooleanOpParityTest.cpp
)
target_include_directories(boolean_op_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(boolean_op_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.ByIdMap
add_executable(byidmap_parity util/ByIdMapParityTest.cpp)
target_include_directories(byidmap_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(byidmap_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.ClampedInt
# ClampedInt parity vs the real net.minecraft.util.valueproviders.ClampedInt.
# VERIFY-EXISTING: the certified port lives in
# world/level/levelgen/IntProvider.h (header-only); the test seeds a
# LegacyRandomSource identically (RandomSource.cpp) and compares sampled +
# bounds results bit-for-bit. Ground truth: tools/ClampedIntParity.java.
add_executable(clamped_int_parity
    util/valueproviders/ClampedIntParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(clamped_int_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(clamped_int_parity PRIVATE bcrypt)
endif()
set_target_properties(clamped_int_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.valueproviders.ClampedNormalFloat
# Bit-exact parity gate for net.minecraft.util.valueproviders.ClampedNormalFloat.
# VERIFY-EXISTING: reuses the certified port in world/level/levelgen/FloatProvider.h
# (mc::valueproviders::ClampedNormalFloat). Pure standard C++; `--cases <tsv>`
# compares against ground truth from tools/ClampedNormalFloatParity.java (the real
# decompiled class). Links RandomSource.cpp for LegacyRandomSource::nextGaussian.
add_executable(clamped_normal_float_parity
    util/valueproviders/ClampedNormalFloatParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(clamped_normal_float_parity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen
)
if(WIN32)
    target_link_libraries(clamped_normal_float_parity PRIVATE bcrypt)
endif()
set_target_properties(clamped_normal_float_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.ColorRGBA
# net.minecraft.util.ColorRGBA parity â€” the typed packed-RGBA value record
# (constructor, rgba() accessor, toString() 8-digit lowercase hex) over a packed
# 32-bit color int, reusing the certified util/ARGB.h channel pack/unpack.
# Header-only (ARGB.h/Mth.h are inline; sRGB/atan LUTs are .inc includes).
# Ground truth: tools/ColorRgbaParity.java.
add_executable(color_rgba_parity
    util/ColorRgbaParityTest.cpp
)
target_include_directories(color_rgba_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(color_rgba_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.Crackiness
add_executable(crackiness_parity world/entity/CrackinessParityTest.cpp)
target_include_directories(crackiness_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(crackiness_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.phys.shapes.CubePointRange
# CubePointRange parity vs the real net.minecraft.world.phys.shapes.CubePointRange
# (DoubleList grid: getDouble(i) = (double)i / parts; size() = parts + 1).
# VERIFY-EXISTING: reuses world/phys/shapes/DoubleList.h (class mc::CubePointRange),
# which is header-only (standard-library includes only), so the test target needs no
# extra sources and no glm. Ground truth: tools/CubePointRangeParity.java.
add_executable(cube_point_range_parity
    world/level/levelgen/CubePointRangeParityTest.cpp
)
target_include_directories(cube_point_range_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cube_point_range_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.phys.shapes.DiscreteCubeMerger
add_executable(cube_voxel_parity world/level/levelgen/DiscreteCubeMergerParityTest.cpp)
target_include_directories(cube_voxel_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cube_voxel_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.Cursor3D
add_executable(cursor3d_parity core/Cursor3DParityTest.cpp)
target_include_directories(cursor3d_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(cursor3d_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.Direction
add_executable(direction_axis_parity core/DirectionAxisExtraParityTest.cpp)
target_include_directories(direction_axis_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(direction_axis_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.Direction8
add_executable(direction8_parity core/Direction8ParityTest.cpp)
target_include_directories(direction8_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(direction8_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# com.mojang.math.Divisor
add_executable(divisor_parity render/model/DivisorParityTest.cpp)
target_include_directories(divisor_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(divisor_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.item.DyeColor
# DyeColor parity vs the real net.minecraft.world.item.DyeColor (16 colors).
# Pure data enum: id/name/textureDiffuseColor/mapColor(id+col)/fireworkColor/textColor,
# plus byId (ByIdMap.continuous ZERO) and byFireworkColor. ARGB.opaque baked into
# the texture/text color constants. Ground truth: tools/DyeColorParity.java.
add_executable(dye_color_parity
    world/item/DyeColorParityTest.cpp
)
target_include_directories(dye_color_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(dye_color_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.Ease
# net.minecraft.util.Ease (the easing primitives: inBack/inBounce/.../inOutBack)
# parity vs the REAL class â€” pure float easings, bit-exact. Header-only port that
# reuses the certified mc::levelgen::mth:: square/cube/sin(table)/cos(table); the
# libm-routed Math.pow/sin/sqrt match after the final narrowing to float.
# Ground truth: tools/EasingParity.java.
add_executable(easing_parity
    util/EasingParityTest.cpp
)
target_include_directories(easing_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(easing_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.EntityAttachments
# EntityAttachments (full Builder.attach + getClamped/get/getAverage/getNullable +
# transformPoint yRot-via-Mth) parity vs the real net.minecraft.world.entity.
# EntityAttachments â€” bit-exact (Mth-table rotation, float 1.0F/size average divisor,
# float->double widening on every attach/scale). Header-only deps (Vec3.h, Mth.h,
# Direction.h), so no extra link libs â€” mirrors vec3_parity. GT: tools/EntityAttachmentsParity.java.
add_executable(entity_attachments_parity
    world/entity/EntityAttachmentsParityTest.cpp
)
target_include_directories(entity_attachments_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(entity_attachments_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.EquipmentSlotGroup
add_executable(equipment_slot_group_parity world/entity/EquipmentSlotGroupParityTest.cpp)
target_include_directories(equipment_slot_group_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(equipment_slot_group_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.EquipmentSlot
add_executable(equipment_slot_parity world/entity/EquipmentSlotParityTest.cpp)
target_include_directories(equipment_slot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(equipment_slot_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.material.FogType
# FogType enum parity vs net.minecraft.world.level.material.FogType (bare enum:
# 5 constants LAVA/WATER/POWDER_SNOW/ATMOSPHERIC/NONE; verifies ordinals + names).
# Ground truth: tools/FogTypeParity.java. Header is self-contained (std only).
add_executable(fog_type_parity
    world/level/material/FogTypeParityTest.cpp
)
target_include_directories(fog_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(fog_type_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.Graph
add_executable(graph_sort_parity util/GraphSortParityTest.cpp)
target_include_directories(graph_sort_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(graph_sort_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.InclusiveRange
add_executable(inclusive_range_parity util/InclusiveRangeParityTest.cpp)
target_include_directories(inclusive_range_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(inclusive_range_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.InterpolationHandler
add_executable(interpolation_handler_parity world/entity/InterpolationHandlerParityTest.cpp)
target_include_directories(interpolation_handler_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(interpolation_handler_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.item.ItemCooldowns
# net.minecraft.world.item.ItemCooldowns parity vs the REAL class. Drives a real
# ItemCooldowns through deterministic op programs (addCooldown(Identifier,int) +
# tick(), both public; removeCooldown public) and records getCooldownPercent /
# isOnCooldown at sampled points; the C++ port (header-only) replays the same op
# stream and must match bit-for-bit. The ItemStack->group resolution
# (getCooldownGroup / addCooldown(ItemStack) / isOnCooldown(ItemStack)) is
# registry/component-coupled and intentionally not exercised â€” raw int group keys.
# Ground truth: tools/ItemCooldownsParity.java.
add_executable(item_cooldowns_parity
    world/item/ItemCooldownsParityTest.cpp
)
target_include_directories(item_cooldowns_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/item)
set_target_properties(item_cooldowns_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.item.ItemDisplayContext
add_executable(item_display_context_parity world/item/ItemDisplayContextParityTest.cpp)
target_include_directories(item_display_context_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(item_display_context_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.Keyframe
# Keyframe interpolation parity (net.minecraft.util.Keyframe / KeyframeTrack /
# KeyframeTrackSampler + EasingType + LerpFunction). Header-only, pure standard
# C++ (no glm/world/registry deps). `--cases <tsv>` compares against ground truth
# from tools/KeyframeParity.java (the real decompiled classes).
add_executable(keyframe_parity
    util/KeyframeParityTest.cpp
)
target_include_directories(keyframe_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(keyframe_parity PRIVATE bcrypt)
endif()
set_target_properties(keyframe_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.LinearCongruentialGenerator
# LinearCongruentialGenerator parity vs the real net.minecraft.util.Linear-
# CongruentialGenerator â€” the positional-random LCG (next(rval,c) +
# MULTIPLIER/INCREMENT) used by BiomeManager's biome-zoom fiddle. Pure 64-bit
# two's-complement long arithmetic, bit-exact. Ground truth: tools/LcgParity.java.
add_executable(lcg_parity
    util/LcgParityTest.cpp
)
target_include_directories(lcg_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(lcg_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.LpVec3
add_executable(lp_vec3_parity
    network/LpVec3ParityTest.cpp
)
target_include_directories(lp_vec3_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(lp_vec3_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.material.MapColor
# MapColor parity vs the real net.minecraft.world.level.material.MapColor â€” the
# map-color col/id table (62 entries) + nested Brightness enum (180/220/255/135)
# + the pure color math: calculateARGBColor / getColorFromPackedId / getPackedId.
# Reuses the certified util/ARGB.h (ARGB.opaque + the int overload of ARGB.scaleRGB).
# Header-only; no glm/json. Ground truth: tools/MapColorParity.java.
add_executable(map_color_parity
    world/level/material/MapColorParityTest.cpp
)
target_include_directories(map_color_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(map_color_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.saveddata.maps.MapDecorationType
add_executable(map_decoration_type_parity world/level/saveddata/maps/MapDecorationTypeParityTest.cpp)
target_include_directories(map_decoration_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(map_decoration_type_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.saveddata.maps.MapId
add_executable(map_id_parity
    world/level/saveddata/maps/MapIdParityTest.cpp
)
target_include_directories(map_id_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/saveddata/maps)
set_target_properties(map_id_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.levelgen.MarsagliaPolarGaussian
# MarsagliaPolarGaussian parity (the polar nextGaussian shared by every
# RandomSource). VERIFY-EXISTING: the class is already ported as the private
# `gaussian()` helper in RandomSource.cpp (surfaced via
# RandomSource::nextGaussian()); this test re-states the standalone class body
# locally and drives the engine RandomSource. `--cases <tsv>` compares against
# ground truth from tools/MarsagliaGaussianParity.java (the real decompiled
# MarsagliaPolarGaussian over the real RandomSource implementations).
add_executable(marsaglia_gaussian_parity
    world/level/levelgen/MarsagliaGaussianParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(marsaglia_gaussian_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
if(WIN32)
    target_link_libraries(marsaglia_gaussian_parity PRIVATE bcrypt)
endif()
set_target_properties(marsaglia_gaussian_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# org.joml.Matrix4f
# org.joml.Matrix4f projection-builder parity (setOrtho / setPerspective /
# perspective) vs the REAL org.joml (joml-1.10.8.jar). Self-contained header
# (render/model/Matrix4fProjection.h) â€” no extra .cpp/lib link closure needed.
# setPerspective routes through org.joml.Math.tan = (float)Math.tan((double)x);
# this gate certifies std::tan agrees bit-for-bit. Ground truth:
# tools/Matrix4fProjParity.java.
add_executable(matrix4f_proj_parity
    render/model/Matrix4fProjParityTest.cpp
)
target_include_directories(matrix4f_proj_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(matrix4f_proj_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.MobCategory
add_executable(mob_category_parity world/entity/MobCategoryParityTest.cpp)
target_include_directories(mob_category_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mob_category_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.MoverType
add_executable(mover_type_parity
    world/entity/MoverTypeParityTest.cpp
)
target_include_directories(mover_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mover_type_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.levelgen.synth.NoiseUtils
add_executable(noise_utils_parity
    world/level/levelgen/NoiseUtilsParityTest.cpp
)
target_include_directories(noise_utils_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
set_target_properties(noise_utils_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.block.state.properties.NoteBlockInstrument
add_executable(note_block_instrument_parity world/level/block/state/properties/NoteBlockInstrumentParityTest.cpp)
target_include_directories(note_block_instrument_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(note_block_instrument_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.phys.shapes.OffsetDoubleList
# OffsetDoubleList parity vs the real net.minecraft.world.phys.shapes.OffsetDoubleList.
# VERIFY-EXISTING: the C++ port lives in world/phys/shapes/DoubleList.h
# (class mc::OffsetDoubleList: getDouble(i) = delegate.getDouble(i) + offset; size() = delegate.size()).
# DoubleList.h is fully header-only (std only), so no extra .cpp link closure is needed.
# Ground truth: tools/OffsetDoubleListParity.java.
add_executable(offset_double_list_parity
    world/level/levelgen/OffsetDoubleListParityTest.cpp
)
target_include_directories(offset_double_list_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(offset_double_list_parity PRIVATE bcrypt)
endif()
set_target_properties(offset_double_list_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.pathfinder.PathType
add_executable(path_type_parity world/level/pathfinder/PathTypeParityTest.cpp)
target_include_directories(path_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(path_type_parity PRIVATE bcrypt)
endif()
set_target_properties(path_type_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.PositionMoveRotation
# PositionMoveRotation (teleport/position-sync record) parity vs the real
# net.minecraft.world.entity.PositionMoveRotation â€” bit-exact. Pure arithmetic
# over the certified Vec3 (xRot/yRot use the Mth sin/cos table) + Mth.clamp(float)
# + the Relative enum mask packing. Header-only closure (Vec3.h -> Mth.h/Direction.h),
# no extra sources, no libs. Ground truth: tools/PositionMoveRotationParity.java.
add_executable(position_move_rotation_parity
    world/entity/PositionMoveRotationParityTest.cpp
)
target_include_directories(position_move_rotation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(position_move_rotation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.material.PushReaction
# PushReaction enum-ordinal/name parity vs net.minecraft.world.level.material.PushReaction
# (bare enum: NORMAL/DESTROY/BLOCK/IGNORE/PUSH_ONLY -> ordinals 0..4). Verifies the
# header-only port world/level/material/PushReaction.h. Ground truth: tools/PushReactionParity.java.
add_executable(push_reaction_parity
    world/level/material/PushReactionParityTest.cpp
)
target_include_directories(push_reaction_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(push_reaction_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.levelgen.RandomSupport
# RandomSupport seed-math parity gate. Verifies the EXISTING certified port in
# world/level/levelgen/RandomSource.{h,cpp}: mixStafford13, upgradeSeedTo128bit(Unmixed),
# Seed128bit::xorWith/mixed, and seedFromHashOf (real MD5_128 -> needs bcrypt on Win).
# Ground truth: mcpp/tools/RandomSupportParity.java (real net.minecraft.world.level.levelgen.RandomSupport).
add_executable(random_support_parity
    world/level/levelgen/RandomSupportParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(random_support_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
if(WIN32)
    target_link_libraries(random_support_parity PRIVATE bcrypt)
endif()
set_target_properties(random_support_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.ai.attributes.RangedAttribute
add_executable(ranged_attribute_parity world/entity/ai/attributes/RangedAttributeParityTest.cpp)
target_include_directories(ranged_attribute_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  target_link_libraries(ranged_attribute_parity PRIVATE bcrypt)
endif()
set_target_properties(ranged_attribute_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.item.Rarity
# Rarity (item rarity enum: id/serialized-name + ChatFormatting color + BY_ID
# ZERO-strategy lookup) parity vs net.minecraft.world.item.Rarity. Pure data
# enum; header is header-only (std lib only). Ground truth: tools/RarityParity.java.
add_executable(rarity_parity
    world/item/RarityParityTest.cpp
)
target_include_directories(rarity_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rarity_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.scores.ReadOnlyScoreInfo
# ReadOnlyScoreInfo interface-contract parity vs net.minecraft.world.scores.ReadOnlyScoreInfo
# (pure subset: int value() / boolean isLocked()). Verifies the reused 1:1 port of its only
# concrete impl, world/scores/Score.h (mc::world::scores::Score), read THROUGH a local model
# of the interface. Header-only â€” no extra link closure. Ground truth: tools/ReadOnlyScoreInfoParity.java.
add_executable(readonly_score_info_parity
    world/scores/ReadOnlyScoreInfoParityTest.cpp
)
target_include_directories(readonly_score_info_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(readonly_score_info_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.core.Rotations
# net.minecraft.core.Rotations parity vs the real 26.1.2 record. Bit-exact: verifies the
# canonical constructor normalization (v % 360.0F for finite; 0 for Inf/NaN) via x()/y()/z()
# accessors, plus re-verifies the reused certified Mth.wrapDegrees(float). Both Rotations.h
# and Mth.h are header-only. Ground truth: tools/RotationsParity.java.
add_executable(rotations_parity
    core/RotationsParityTest.cpp
)
target_include_directories(rotations_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rotations_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.scores.Score
add_executable(score_parity world/scores/ScoreParityTest.cpp)
target_include_directories(score_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(score_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.SegmentedAnglePrecision
# SegmentedAnglePrecision angle-quantization parity vs the real
# net.minecraft.util.SegmentedAnglePrecision (toSegment/fromSegment style binary-angle
# math used by RotationSegment / rotated blocks). Pure int/float, bit-exact (floats as
# raw IEEE-754 bits; Math.round(float) via the JDK bit-twiddle). Ground truth:
# tools/SegAngleParity.java.
add_executable(seg_angle_parity
    util/SegAngleParityTest.cpp
)
target_include_directories(seg_angle_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(seg_angle_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.AnimationState
add_executable(squid_swim_parity
    world/entity/AnimationStateParityTest.cpp
)
target_include_directories(squid_swim_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/world/entity)
set_target_properties(squid_swim_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.StringUtil
# Pure-string helpers parity vs the real net.minecraft.util.StringUtil
# (isBlank/isNullOrEmpty/truncateStringIfNecessary/lineCount/trimChatMessage/
# isAllowedChatCharacter/isWhitespace). Header-only (std lib + embedded JDK whitespace
# table StringUtilWhitespace.inc); no project link deps. Ground truth: tools/StringUtilParity.java.
add_executable(stringutil_parity util/StringUtilParityTest.cpp)
target_include_directories(stringutil_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(stringutil_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.item.ToolMaterial
add_executable(tool_material_parity world/item/ToolMaterialParityTest.cpp)
target_include_directories(tool_material_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(tool_material_parity PRIVATE bcrypt)
endif()
set_target_properties(tool_material_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.TrapezoidFloat
add_executable(trapezoid_float_parity
    util/valueproviders/TrapezoidFloatParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(trapezoid_float_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(trapezoid_float_parity PRIVATE bcrypt)
endif()
set_target_properties(trapezoid_float_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.TriState
# net.minecraft.util.TriState parity gate (pure value enum: from / toBoolean /
# getSerializedName). Pure standard C++, no deps; `--cases <tsv>` compares against
# ground truth from tools/TriStateParity.java (the real decompiled TriState).
add_executable(tristate_parity
    util/TriStateParityTest.cpp
)
target_include_directories(tristate_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/util)
set_target_properties(tristate_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.valueproviders.UniformFloat
# UniformFloat value-provider parity vs net.minecraft.util.valueproviders.UniformFloat.
# VERIFIES the existing certified port mc::valueproviders::UniformFloat (in
# world/level/levelgen/FloatProvider.h); sample == nextFloat()*(max-min)+min.
# Ground truth: tools/UniformFloatParity.java (real UniformFloat over LEG+XOR RNGs).
add_executable(uniform_float_parity
    util/valueproviders/UniformFloatParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(uniform_float_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(uniform_float_parity PRIVATE bcrypt)
endif()
set_target_properties(uniform_float_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.valueproviders.UniformInt
# UniformInt parity GATE vs the REAL net.minecraft.util.valueproviders.UniformInt.
# Verifies the existing certified port mc::valueproviders::UniformInt in
# world/level/levelgen/IntProvider.h (header-only). `--cases <tsv>` compares
# bit-for-bit against ground truth from tools/UniformIntParity.java. Link closure:
# only RandomSource.cpp (LegacyRandomSource + XoroshiroRandomSource) + bcrypt on WIN32.
add_executable(uniform_int_parity
    util/valueproviders/UniformIntParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(uniform_int_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(uniform_int_parity PRIVATE bcrypt)
endif()
set_target_properties(uniform_int_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.core.UUIDUtil
add_executable(uuidutil_parity core/UuidUtilParityTest.cpp)
target_include_directories(uuidutil_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(uuidutil_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.network.VarInt
# net.minecraft.network.VarInt parity: getByteSize(int) + encode bytes (VarInt.write)
# over a wide int range. Ground truth: tools/VarIntSizeParity.java.
# Also pins the existing PacketBuffer::writeVarInt encoder (same bytes).
add_executable(varint_size_parity
    world/level/levelgen/VarIntSizeParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(varint_size_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(varint_size_parity PRIVATE miniz)
set_target_properties(varint_size_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.VarLong
add_executable(varlong_size_parity
    world/level/levelgen/VarLongSizeParityTest.cpp
)
target_include_directories(varlong_size_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(varlong_size_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.Avatar
add_executable(vec2_done_parity util/AvatarParityTest.cpp)
target_include_directories(vec2_done_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec2_done_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.levelgen.VerticalAnchor
add_executable(vertical_anchor_parity
    ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen/VerticalAnchorParityTest.cpp)
target_include_directories(vertical_anchor_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vertical_anchor_parity PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.WalkAnimationState
add_executable(walk_anim_parity world/entity/WalkAnimParityTest.cpp)
target_include_directories(walk_anim_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(walk_anim_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.WeightedListInt
# WeightedListInt parity. VERIFIES the existing engine port
# mc::valueproviders::WeightedListInt in world/level/levelgen/IntProvider.h
# (the test #includes it via a relative path). Pure standard C++; `--cases <tsv>`
# compares against ground truth from tools/WeightedListIntParity.java (the real
# decompiled net.minecraft.util.valueproviders.WeightedListInt / WeightedList).
# Link closure: only RandomSource.cpp (the certified LegacyRandomSource) + bcrypt
# on Windows; IntProvider.h pulls in nothing else (no glm/AABB/Vec3).
add_executable(weighted_list_int_parity
    util/valueproviders/WeightedListIntParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(weighted_list_int_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(weighted_list_int_parity PRIVATE bcrypt)
endif()
set_target_properties(weighted_list_int_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.block.state.properties.WoodType
add_executable(wood_type_parity world/level/block/state/properties/WoodTypeParityTest.cpp)
target_include_directories(wood_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(wood_type_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus
add_executable(xoroshiro128_parity world/level/levelgen/Xoroshiro128ParityTest.cpp world/level/levelgen/RandomSource.cpp)
target_include_directories(xoroshiro128_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(xoroshiro128_parity PRIVATE bcrypt)
endif()
set_target_properties(xoroshiro128_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== Parallel-authored bounded gates (workflow wave 5) =====

# net.minecraft.world.entity.ai.attributes.AttributeModifier
# net.minecraft.world.entity.ai.attributes.AttributeModifier.Operation
# (enum id/ordinal/name/getSerializedName) + AttributeInstance value-combine formula.
# Header-only: reuses the certified AttributeMath.h and the new AttributeOperation.h;
# no extra link sources, no glm/bcrypt needed (pure std).
add_executable(attribute_op_parity world/entity/ai/attributes/AttributeModifierOpParityTest.cpp)
target_include_directories(attribute_op_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(attribute_op_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.AxisCycle
add_executable(axis_cycle_parity core/AxisCycleParityTest.cpp)
target_include_directories(axis_cycle_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(axis_cycle_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.BlockPos
add_executable(block_pos_verify_parity world/level/levelgen/BlockPosVerifyParityTest.cpp)
target_include_directories(block_pos_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(block_pos_verify_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.Brightness
add_executable(brightness_verify_parity world/level/levelgen/BrightnessVerifyParityTest.cpp)
target_include_directories(brightness_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(brightness_verify_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.block.state.properties (BambooLeaves/BellAttachType/DripstoneThickness/CreakingHeartState/SculkSensorPhase/SideChainPart/StructureMode/TestBlockMode)
# net.minecraft.world.level.block.state.properties (BambooLeaves/BellAttachType/DripstoneThickness/CreakingHeartState/SculkSensorPhase/SideChainPart/StructureMode/TestBlockMode)
add_executable(bsenums2_parity world/level/block/state/properties/BlockStateEnums2ParityTest.cpp)
target_include_directories(bsenums2_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(bsenums2_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.ChatFormatting
# net.minecraft.ChatFormatting
# ChatFormatting (22-value color/format enum: char code / id / isFormat / isColor /
# nullable RGB color + getByCode/getById/getByName static lookups) parity vs
# net.minecraft.ChatFormatting. Pure data enum; header is header-only (std lib only).
# Ground truth: tools/ChatFormattingParity.java.
add_executable(chat_formatting_parity
    world/level/levelgen/ChatFormattingParityTest.cpp
)
target_include_directories(chat_formatting_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  target_link_libraries(chat_formatting_parity PRIVATE bcrypt)
endif()
set_target_properties(chat_formatting_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.player.ChatVisiblity
# net.minecraft.world.entity.player.ChatVisiblity
# ChatVisiblity enum parity gate (FULL/SYSTEM/HIDDEN: ordinal/name/private `id` field/
# translation key + the ByIdMap.continuous(WRAP) LEGACY_CODEC decode). Pure standard
# C++17, no deps; `--cases <tsv>` compares against ground truth from
# tools/ChatVisibilityParity.java (the real decompiled ChatVisiblity via reflection).
add_executable(chat_visibility_parity
    world/entity/player/ChatVisibilityParityTest.cpp
)
target_include_directories(chat_visibility_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(chat_visibility_parity PRIVATE bcrypt)
endif()
set_target_properties(chat_visibility_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.chat.ClickEvent.Action
# net.minecraft.network.chat.ClickEvent.Action
add_executable(click_event_action_parity network/chat/ClickEventActionParityTest.cpp)
target_include_directories(click_event_action_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(click_event_action_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.levelgen.Column
add_executable(column_parity world/level/levelgen/ColumnParityTest.cpp)
target_include_directories(column_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(column_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.CommonColors
# Named ARGB color constants parity vs the real net.minecraft.util.CommonColors
# (WHITE/BLACK/GRAY/RED/GREEN/BLUE/... packed 0xAARRGGBB ints used by GUI/text).
# Pure data, header-only (util/CommonColors.h). Ground truth: tools/CommonColorsParity.java.
add_executable(common_colors_parity
    util/CommonColorsParityTest.cpp
)
target_include_directories(common_colors_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(common_colors_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.CrudeIncrementalIntIdentityHashBiMap
add_executable(crude_bimap_parity util/CrudeBiMapParityTest.cpp)
target_include_directories(crude_bimap_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(crude_bimap_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.Difficulty
# net.minecraft.world.Difficulty
add_executable(difficulty_parity world/DifficultyParityTest.cpp)
target_include_directories(difficulty_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(difficulty_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.Direction
# net.minecraft.core.Direction (2D-data / yRot / opposite / getAxisDirection / Plane iteration â€” verify-existing)
add_executable(direction_plane_parity core/DirectionPlaneParityTest.cpp)
target_include_directories(direction_plane_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(direction_plane_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.scores.DisplaySlot
# net.minecraft.world.scores.DisplaySlot
add_executable(display_slot_parity world/scores/DisplaySlotParityTest.cpp)
target_include_directories(display_slot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(display_slot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.GameType
# GameType enum parity vs the real net.minecraft.world.level.GameType â€” the pure
# methods only (getId/byId(ZERO strategy)/byName(EnumCodec linear scan)/isCreative/
# isSurvival/isBlockPlacingRestricted/getName/getSerializedName + getNullableId/
# byNullableId/isValidId). Header-only port, bit-exact. GT: tools/GameTypeParity.java.
add_executable(game_type_parity
    world/level/GameTypeParityTest.cpp
)
target_include_directories(game_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(game_type_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.levelgen.GenerationStep
add_executable(generation_step_parity world/level/levelgen/GenerationStepParityTest.cpp)
target_include_directories(generation_step_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(generation_step_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.GlobalPos
# GlobalPos (net.minecraft.core.GlobalPos) parity vs the real 26.1.2 record â€”
# byte-deterministic surface: toString, equals, isCloseEnough, and the BlockPos/Vec3i
# helpers it relies on (asLong packed pos, hashCode, distChessboard). Ground truth:
# tools/GlobalPosParity.java. (GlobalPos.hashCode is identity-based -> non-deterministic,
# intentionally skipped; codecs out of scope.)
add_executable(global_pos_parity
    core/GlobalPosParityTest.cpp
)
target_include_directories(global_pos_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(global_pos_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.levelgen.Heightmap.Types
# net.minecraft.world.level.levelgen.Heightmap.Types (enum part only)
# Heightmap.Types parity: per-constant (ordinal/id/serializedName/getSerializationKey/
# usage.ordinal/usage.name/sendToClient/keepAfterWorldgen/isOpaque-predicate-category)
# + Types.BY_ID (ByIdMap.continuous / ZERO) lookup. Header-only (std lib only; no
# heightmap data array / BitStorage). Ground truth: tools/HeightmapTypesParity.java.
add_executable(heightmap_types_parity
    world/level/levelgen/HeightmapTypesParityTest.cpp
)
target_include_directories(heightmap_types_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
  target_link_libraries(heightmap_types_parity PRIVATE bcrypt)
endif()
set_target_properties(heightmap_types_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.chat.HoverEvent.Action
# net.minecraft.network.chat.HoverEvent.Action
add_executable(hover_event_action_parity network/chat/HoverEventActionParityTest.cpp)
target_include_directories(hover_event_action_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(hover_event_action_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.HumanoidArm
# net.minecraft.world.entity.HumanoidArm
add_executable(humanoid_arm_parity
    world/entity/HumanoidArmParityTest.cpp
)
target_include_directories(humanoid_arm_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(humanoid_arm_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.InteractionHand
# net.minecraft.world.InteractionHand
# InteractionHand enum parity vs the real net.minecraft.world.InteractionHand
# (bare 2-constant enum MAIN_HAND(0)/OFF_HAND(1); verifies ordinals + names +
# the private `id` field + BY_ID ByIdMap.continuous(ZERO) mapper +
# asEquipmentSlot() -> EquipmentSlot ordinal). Header world/InteractionHand.h is
# self-contained (std only). Ground truth: tools/InteractionHandParity.java.
add_executable(interaction_hand_parity world/InteractionHandParityTest.cpp)
target_include_directories(interaction_hand_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(interaction_hand_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.LightLayer
# net.minecraft.world.level.LightLayer
add_executable(light_layer_parity world/level/LightLayerParityTest.cpp)
target_include_directories(light_layer_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(light_layer_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.ai.memory.MemoryStatus
# net.minecraft.world.entity.ai.memory.MemoryStatus
add_executable(memory_status_parity
    world/entity/ai/memory/MemoryStatusParityTest.cpp
)
target_include_directories(memory_status_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(memory_status_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.block.Mirror
# Mirror parity vs net.minecraft.world.level.block.Mirror (pure StringRepresentable
# enum: mirror(int,int) / getRotation(Direction) / mirror(Direction) + ordinal/name/
# getSerializedName). Verifies the certified Mirror port already living in
# StructureTransforms.h, plus the id strings from MirrorSerializedName.h. Header-only;
# ground truth: tools/MirrorParity.java.
add_executable(mirror_parity
    world/level/block/MirrorParityTest.cpp
)
target_include_directories(mirror_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mirror_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.Mth
# Extra Mth parity: the remaining net.minecraft.util.Mth helpers not directly gated by
# mth_parity â€” wrapDegrees(float/double), degreesDifference, positiveModulo(int/float/
# double), clamp(int/float/double), length(x,y) (double+float), lengthSquared(x,y) /
# lengthSquared(x,y,z), getSeed(x,y,z), floorDiv, floorMod. Bit-exact vs the real
# 26.1.2 class. Ground truth: tools/MthExtraParity.java. Verifies the existing engine
# header world/level/levelgen/Mth.h (+ a local corrected length(float,float) helper).
add_executable(mth_extra_parity
    world/level/levelgen/MthExtraParityTest.cpp
)
target_include_directories(mth_extra_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mth_extra_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.entity.Pose
# net.minecraft.world.entity.Pose
add_executable(pose_parity world/entity/PoseParityTest.cpp)
target_include_directories(pose_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(pose_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.biome.Biome.Precipitation
# net.minecraft.world.level.biome.Biome.Precipitation
# Nested enum (NONE/RAIN/SNOW) parity vs the real Biome.Precipitation: per
# constant ordinal()/name()/getSerializedName() + values().length. Header-only
# (std lib); no project link deps. Ground truth: tools/PrecipitationParity.java.
add_executable(precipitation_parity world/level/biome/PrecipitationParityTest.cpp)
target_include_directories(precipitation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(precipitation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.QuartPos
add_executable(quart_pos_verify_parity world/level/levelgen/QuartPosVerifyParityTest.cpp)
target_include_directories(quart_pos_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(quart_pos_verify_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.Relative
# net.minecraft.world.entity.Relative (a.k.a. RelativeMovement) â€” pack/unpack bitset
add_executable(relative_movement_parity
    world/entity/RelativeMovementParityTest.cpp
)
target_include_directories(relative_movement_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(relative_movement_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.block.Rotation
# Block Rotation parity vs net.minecraft.world.level.block.Rotation.
# VERIFY-EXISTING: the functional Rotation port (enum + getRotated/rotate(Direction)/
# rotate(int,int)) already lives, certified, in StructureTransforms.h; this gate
# re-includes that header (no edits) and adds the serialized-name/index checks.
# Header-only (StructureTransforms.h -> Direction.h are both header-only, pure
# integer math) â€” no glm/json/extra sources needed.
# Ground truth: tools/BlockRotationParity.java.
add_executable(rotation_block_parity
    world/level/block/BlockRotationParityTest.cpp
)
target_include_directories(rotation_block_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rotation_block_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.block.state.properties.RotationSegment
# net.minecraft.world.level.block.state.properties.RotationSegment
# RotationSegment 16-segment (4-bit binary angle) quantization parity vs the real
# net.minecraft.world.level.block.state.properties.RotationSegment. A thin static
# wrapper over the already-certified SegmentedAnglePrecision(4) port (util/
# SegmentedAnglePrecision.h, gated by seg_angle_parity). Pure int/float, bit-exact
# (floats as raw IEEE-754 bits; Math.round(float) via the JDK bit-twiddle).
# Ground truth: tools/RotationSegmentParity.java.
add_executable(rotation_segment_parity
    world/level/block/state/properties/RotationSegmentParityTest.cpp
)
target_include_directories(rotation_segment_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rotation_segment_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.core.SectionPos
# SectionPos parity vs the real net.minecraft.core.SectionPos â€” long bit-packing
# (asLong / x/y/z), block<->section coord conversion (reused read-only from
# core/PosCodec.h, already certified by pos_codec_parity) plus the relative/offset
# surface newly ported in core/SectionPosOffset.h. Pure int/long, bit-exact.
# Ground truth: tools/SectionPosParity.java.
add_executable(section_pos_parity
    core/SectionPosParityTest.cpp
)
target_include_directories(section_pos_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(section_pos_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.sounds.SoundSource
# net.minecraft.sounds.SoundSource
add_executable(sound_source_parity sounds/SoundSourceParityTest.cpp)
target_include_directories(sound_source_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sound_source_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.stats.StatFormatter
# net.minecraft.stats.StatFormatter
# Pure int->String formatter parity vs the real net.minecraft.stats.StatFormatter
# constants (DEFAULT / DIVIDE_BY_TEN / DISTANCE / TIME). The header reproduces the
# three underlying JDK number->string primitives with EXACT integer/decimal
# arithmetic from the IEEE-754 double bits (no float approximation):
#   NumberFormat.getIntegerInstance(Locale.US).format  (US ',' grouping),
#   DecimalFormat("########0.00", ROOT).format          (2-frac HALF_EVEN),
#   Double.toString                                      (shortest round-trip / Dragon4).
# Header-only (std lib only); no project link deps. Ground truth: tools/StatFormatterParity.java.
add_executable(stat_formatter_parity stats/StatFormatterParityTest.cpp)
target_include_directories(stat_formatter_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stats)
set_target_properties(stat_formatter_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.phys.Vec2
add_executable(vec2_verify_parity world/level/levelgen/Vec2VerifyParityTest.cpp)
target_include_directories(vec2_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec2_verify_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.random.Weighted
# net.minecraft.util.random.Weighted (record) + WeightedList.getRandom selection.
# NEW gate. Ports the Weighted<T> record (accessors / map / weight<0 validation)
# in the NEW header util/random/Weighted.h, and REUSES read-only the existing
# certified selection port util/WeightedRandom.h (Flat/Compact). The existing
# weighted_random_parity target (CMakeLists ~line 814) already gates the
# WeightedRandom/WeightedList selection algorithms; this target adds the record.
# Link closure: only RandomSource.cpp (certified LegacyRandomSource) + bcrypt on
# Windows. Neither Weighted.h nor WeightedRandom.h pulls in glm/AABB/Vec3.
add_executable(weighted_parity
    util/random/WeightedParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(weighted_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(weighted_parity PRIVATE bcrypt)
endif()
set_target_properties(weighted_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== Parallel-authored bounded gates (workflow wave 6) =====

# net.minecraft.util.ARGB
# Second ARGB parity GATE â€” VERIFIES the existing certified port util/ARGB.h against
# fresh ground truth. Focus: color(a,r,g,b)/red/green/blue/alpha, srgbLerp/linearLerp,
# multiply, scaleRGB (all overloads), opaque, colorFromFloat, PLUS the two-arg
# color(int,int)/color(float,int) overloads the original argb_parity gate did not
# exercise. Header-only (ARGB.h + Mth.h + embedded ARGBTables.inc are all inline), so
# no extra sources. Ground truth: tools/ArgbVerifyParity.java.
add_executable(argb_verify_parity
    world/level/levelgen/ArgbVerifyParityTest.cpp
)
target_include_directories(argb_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(argb_verify_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.world.level.ClipContext
# net.minecraft.world.level.ClipContext (nested enums Block + Fluid: ordinal()/name())
add_executable(clip_context_enums_parity world/level/ClipContextEnumsParityTest.cpp)
target_include_directories(clip_context_enums_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(clip_context_enums_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.Crypt
# net.minecraft.util.Crypt (PURE byte helpers: digestData SHA-1 + server-id hash)
# Verifies mc::crypt in util/Crypt.h (header-only): genuine RFC 3174 SHA-1 over the
# concatenated byte inputs of Crypt.digestData(byte[]...), plus the signed-BigInteger
# "server id hash" string (login line 182). `--cases <tsv>` compares byte-for-byte
# against ground truth from tools\CryptDigestParity.java. No extra link sources.
add_executable(crypt_digest_parity util/CryptDigestParityTest.cpp)
target_include_directories(crypt_digest_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(crypt_digest_parity PRIVATE bcrypt)
endif()
set_target_properties(crypt_digest_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.CubicSpline
add_executable(cubic_spline_verify_parity
    world/level/levelgen/CubicSplineVerifyParityTest.cpp
    world/level/levelgen/Noise.cpp
    world/level/levelgen/DensityFunction.cpp
    world/level/levelgen/CubicSpline.cpp
    world/level/levelgen/RandomSource.cpp)
target_include_directories(cubic_spline_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/world/level/levelgen)
target_link_libraries(cubic_spline_verify_parity PRIVATE glm nlohmann_json)
if(WIN32)
    target_link_libraries(cubic_spline_verify_parity PRIVATE bcrypt)
endif()
set_target_properties(cubic_spline_verify_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.DependencySorter
add_executable(dependency_sorter_parity util/DependencySorterParityTest.cpp)
target_include_directories(dependency_sorter_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(dependency_sorter_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.entity.EntityDimensions
# VERIFY gate for net.minecraft.world.entity.EntityDimensions (26.1.2) â€” an
# independent re-verification of the already-certified header
# mcpp/src/world/entity/EntityDimensions.h (no new header; verify-existing).
# Exercises scalable/fixed/scale(f)/scale(fw,fh)/withEyeHeight/makeBoundingBox +
# the default EntityAttachments points, bit-exact. Header-only deps (AABB.h pulls
# in glm::dvec3, the only link lib needed â€” mirrors entity_dimensions_parity).
# GT: tools/EntityDimensionsVerifyParity.java.
add_executable(entity_dimensions_verify_parity world/level/levelgen/EntityDimensionsVerifyParityTest.cpp)
target_include_directories(entity_dimensions_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(entity_dimensions_verify_parity PRIVATE glm)
set_target_properties(entity_dimensions_verify_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.InteractionResult
add_executable(interaction_result_parity world/InteractionResultParityTest.cpp)
target_include_directories(interaction_result_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(interaction_result_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.item.ItemUseAnimation
# net.minecraft.world.item.ItemUseAnimation
# Pure-data StringRepresentable enum (12 constants): id / ordinal /
# getSerializedName / hasCustomArmTransform, plus the BY_ID lookup
# (ByIdMap.continuous, ZERO strategy). Header-only, pure standard C++ (no
# glm/world/registry deps). `--cases <tsv>` compares against ground truth from
# tools/ItemUseAnimationParity.java (the real decompiled enum).
add_executable(item_use_animation_parity world/item/ItemUseAnimationParityTest.cpp)
target_include_directories(item_use_animation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(item_use_animation_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.Mth
# mth_color_verify_parity â€” VERIFIES the colour/hash helpers already ported in
# world/level/levelgen/Mth.h (mc::levelgen::mth): hsvToRgb / hsvToArgb /
# murmurHash3Mixer / binarySearch / frac(float/double) / smoothstep /
# smoothstepDerivative, plus net.minecraft.util.ARGB.color(a,r,g,b) against a
# corrected masked reference. Bit-exact vs the real 26.1.2 class. Header-only port;
# no link sources. Ground truth: tools/MthColorVerifyParity.java.
add_executable(mth_color_verify_parity
    world/level/levelgen/MthColorVerifyParityTest.cpp
)
target_include_directories(mth_color_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(mth_color_verify_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundChatCommandPacket
# Byte-exact parity for net.minecraft.network.protocol.game.ServerboundChatCommandPacket
# (UNSIGNED chat-command packet) vs the REAL STREAM_CODEC. Codec is exactly
# writeUtf(command)/readUtf(); reuses the certified PacketBuffer (Utf8String port).
# Ground truth: tools/PktChatCommandParity.java.
add_executable(pkt_chat_command_parity
    network/protocol/game/PktChatCommandParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_chat_command_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_chat_command_parity PRIVATE miniz)
set_target_properties(pkt_chat_command_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundClientCommandPacket
add_executable(pkt_client_command_parity
    network/protocol/game/PktClientCommandParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_client_command_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_client_command_parity PRIVATE miniz)
set_target_properties(pkt_client_command_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ClientboundKeepAlivePacket
# ClientboundKeepAlivePacket: byte-exact packet codec parity vs the REAL
# net.minecraft STREAM_CODEC. A single big-endian 64-bit long (id) â€” verified
# against the Java: write -> FriendlyByteBuf.writeLong, read -> readLong (no VarLong).
add_executable(pkt_keepalive_cb_parity
    network/protocol/common/PktKeepAliveCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_keepalive_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_keepalive_cb_parity PRIVATE miniz)
set_target_properties(pkt_keepalive_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ServerboundKeepAlivePacket
# ServerboundKeepAlivePacket codec parity vs the REAL StreamCodec/FriendlyByteBuf.
# The packet body is exactly one writeLong (8 big-endian bytes); byte-match.
# Ground truth: tools/PktKeepAliveSbParity.java.
add_executable(pkt_keepalive_sb_parity
    network/protocol/common/PktKeepAliveSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_keepalive_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_keepalive_sb_parity PRIVATE miniz)
set_target_properties(pkt_keepalive_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ClientboundPingPacket
# net.minecraft.network.protocol.common.ClientboundPingPacket â€” packet codec
# byte parity vs the real STREAM_CODEC. Body is a single fixed-width big-endian
# int (FriendlyByteBuf.writeInt). Ground truth: tools/PktPingCbParity.java.
add_executable(pkt_ping_cb_parity
    network/protocol/common/PktPingCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_ping_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_ping_cb_parity PRIVATE miniz)
set_target_properties(pkt_ping_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ServerboundPongPacket
# ServerboundPongPacket codec parity vs the REAL net.minecraft StreamCodec.
# Body is a single FriendlyByteBuf.writeInt(id) -> 4-byte big-endian int; the C++
# PacketBuffer must reproduce the bytes exactly and round-trip-decode them.
# Ground truth: tools/PktPongSbParity.java.
add_executable(pkt_pong_sb_parity
    network/protocol/common/PktPongSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_pong_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_pong_sb_parity PRIVATE miniz)
set_target_properties(pkt_pong_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSetCarriedItemPacket
# ServerboundSetCarriedItemPacket StreamCodec parity vs the REAL net.minecraft
# codec. Body is exactly FriendlyByteBuf.writeShort(slot)/readShort() (low 16 bits
# big-endian on write; signed 16-bit sign-extended on read). Verifies the certified
# PacketBuffer (FriendlyByteBuf port) directly. Ground truth: tools/PktSetCarriedParity.java.
add_executable(pkt_set_carried_parity
    network/protocol/game/PktSetCarriedParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_carried_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_carried_parity PRIVATE miniz)
set_target_properties(pkt_set_carried_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetTimePacket
# Byte-exact parity for ClientboundSetTimePacket vs the real STREAM_CODEC
# (long gameTime + Map<Holder<WorldClock>, ClockNetworkState> clockUpdates).
# Ground truth: tools/PktSetTimeParity.java (encodes through RegistryFriendlyByteBuf
# with a real WORLD_CLOCK registry; overworld id 0, the_end id 1).
add_executable(pkt_set_time_parity
    network/protocol/game/PktSetTimeParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_time_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_time_parity PRIVATE miniz)
set_target_properties(pkt_set_time_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSwingPacket
# ServerboundSwingPacket StreamCodec parity vs the REAL net.minecraft codec.
# Body == FriendlyByteBuf.writeEnum(hand) == writeVarInt(hand.ordinal());
# InteractionHand MAIN_HAND=0 / OFF_HAND=1 -> wire 0x00 / 0x01.
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktSwingParity.java.
add_executable(pkt_swing_parity
    network/protocol/game/PktSwingParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_swing_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_swing_parity PRIVATE miniz)
set_target_properties(pkt_swing_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ClientboundTransferPacket
# ClientboundTransferPacket codec parity vs the REAL STREAM_CODEC: writeUtf(host)
# + writeVarInt(port), byte-exact + read round trip. Ground truth:
# tools/PktTransferCbParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_transfer_cb_parity
    network/protocol/common/PktTransferCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_transfer_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_transfer_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_transfer_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_transfer_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.StringDecomposer
# net.minecraft.util.StringDecomposer
add_executable(string_decomposer_parity util/StringDecomposerParityTest.cpp)
target_include_directories(string_decomposer_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(string_decomposer_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.core.Vec3i
# net.minecraft.core.Vec3i (verify-existing: core/Vec3i.h, header-only deps)
add_executable(vec3i_verify_parity world/level/levelgen/Vec3iVerifyParityTest.cpp)
target_include_directories(vec3i_verify_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(vec3i_verify_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
