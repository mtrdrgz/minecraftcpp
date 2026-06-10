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

# ===== Parallel-authored packet-codec gates (workflow wave 7, netcode) =====

# net.minecraft.network.protocol.game.ClientboundAnimatePacket
# Byte-exact parity for ClientboundAnimatePacket vs the REAL STREAM_CODEC.
# Body == FriendlyByteBuf.writeVarInt(id) + writeByte(action); read ==
# readVarInt() + readUnsignedByte(). Both fields are plain numbers, fully
# representable by the certified PacketBuffer (FriendlyByteBuf port) â€” no
# registry/ItemStack/Component/Holder/NBT. GT: tools/PktAnimateCbParity.java.
add_executable(pkt_animate_cb_parity
    network/protocol/game/PktAnimateCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_animate_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_animate_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_animate_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_animate_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundBlockChangedAckPacket
# ClientboundBlockChangedAckPacket StreamCodec parity vs the REAL net.minecraft codec.
# The packet is record(int sequence); body == FriendlyByteBuf.writeVarInt(sequence)
# (read == readVarInt()), no packet-id prefix (Packet.codec/StreamCodec.ofMember).
# Reuses the certified PacketBuffer (FriendlyByteBuf port): writeVarInt/readVarInt are
# byte-/value-exact. GT: tools/PktBlockChangedAckParity.java.
add_executable(pkt_block_changed_ack_parity
    network/protocol/game/PktBlockChangedAckParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_block_changed_ack_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_block_changed_ack_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_block_changed_ack_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_block_changed_ack_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundBlockDestructionPacket
# Byte-exact parity for the ClientboundBlockDestructionPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ClientboundBlockDestructionPacket.java:29-33):
#   writeVarInt(id) + writeBlockPos(pos)[== writeLong(pos.asLong())] + writeByte(progress).
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix. BlockPos.asLong
# is the 26/12/26 big-endian long packing (BlockPos.java:107-116). Reuses the certified
# PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktBlockDestructionParity.java.
add_executable(pkt_block_destruction_parity
    network/protocol/game/PktBlockDestructionParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_block_destruction_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_block_destruction_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_block_destruction_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_block_destruction_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundBlockEventPacket
# Byte-exact parity for ClientboundBlockEventPacket vs the REAL STREAM_CODEC.
# Body (write(RegistryFriendlyByteBuf), lines 35-40):
#   writeBlockPos(pos) == writeLong(pos.asLong())  (BE 8-byte long)
#   writeByte(b0) ; writeByte(b1)                   (one byte each, low 8 bits)
#   ByteBufCodecs.registry(BLOCK).encode == writeVarInt(registry id)
# The block is a plain INT registry id on the wire (NOT a Holder/ResourceLocation),
# so the certified PacketBuffer (FriendlyByteBuf port) rebuilds the body directly.
# Ground truth: tools/PktBlockEventParity.java (encodes through RegistryFriendlyByteBuf
# with a RegistryAccess over BuiltInRegistries.BLOCK; emits posLong/b0/b1/blockId/hex).
add_executable(pkt_block_event_parity
    network/protocol/game/PktBlockEventParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_block_event_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_block_event_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_block_event_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_block_event_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.handshake.ClientIntentionPacket
# Byte-exact parity vs the REAL ClientIntentionPacket.STREAM_CODEC. Codec body is
# writeVarInt(protocolVersion); writeUtf(hostName); writeShort(port);
# writeVarInt(intention.id())  [id() = 1/2/3 STATUS/LOGIN/TRANSFER, NOT ordinal].
# Reuses the certified PacketBuffer (FriendlyByteBuf port); links its full closure
# (NbtIo/Tag for writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktClientIntentionParity.java.
add_executable(pkt_client_intention_parity
    network/protocol/handshake/PktClientIntentionParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_client_intention_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_client_intention_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_client_intention_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_client_intention_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundContainerClosePacket
# Byte-exact parity for ServerboundContainerClosePacket vs the real STREAM_CODEC:
# the body is exactly writeContainerId(containerId) == VarInt.write of the signed int
# (FriendlyByteBuf lines 671-685), so the whole payload is a single VarInt. Ground
# truth: tools/PktContainerCloseSbParity.java. Reuses the certified PacketBuffer
# (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for writeNbt
# symbols) + miniz (+ bcrypt on WIN32).
add_executable(pkt_container_close_sb_parity
    network/protocol/game/PktContainerCloseSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_close_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_close_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_close_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_close_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.cookie.ClientboundCookieRequestPacket
# Codec is a single writeIdentifier(key) == writeUtf(identifier.toString())
# where Identifier.toString() == "namespace:path". Reuses the certified
# mc::net::PacketBuffer (Utf8String port: writeString==writeUtf/readString==readUtf).
# Ground truth: tools/PktCookieRequestCbParity.java. Links the PacketBuffer
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz; bcrypt on WIN32.
add_executable(pkt_cookie_request_cb_parity
    network/protocol/cookie/PktCookieRequestCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_cookie_request_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_cookie_request_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_cookie_request_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_cookie_request_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Byte-exact parity for net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket
# vs the REAL STREAM_CODEC. The codec is ByteBufCodecs.lenientJson(262144).apply(
# fromCodec(OPS, ComponentSerialization.CODEC)): the Component is serialized to a
# canonical JSON string (Mojang codec + Gson disableHtmlEscaping) and written on the
# wire purely via Utf8String.write == writeUtf (VarInt UTF-8 byte length + bytes). No
# registry/NBT/Holder/ItemStack data reaches the wire (RegistryAccess.EMPTY + JsonOps),
# so the C++ side reproduces writeUtf(jsonString) on the exact canonical string carried
# in the TSV jsonHex column. Reuses the certified PacketBuffer (FriendlyByteBuf port);
# links its full closure (NbtIo/Tag for writeNbt symbols) + miniz, bcrypt on WIN32.
# Ground truth: tools/PktLoginDisconnectCbParity.java.
add_executable(pkt_disconnect_login_cb_parity
    network/protocol/login/PktLoginDisconnectCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_disconnect_login_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_disconnect_login_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_disconnect_login_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_disconnect_login_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundEntityEventPacket
# Byte-exact parity for ClientboundEntityEventPacket vs the real STREAM_CODEC:
#   write: writeInt(entityId) [big-endian 4 bytes] + writeByte(eventId) [signed byte]
#   read : readInt() + readByte() (signed). Packet.codec -> StreamCodec.ofMember,
#   so no packet-id prefix on the wire, just the 5-byte body.
# Ground truth: tools/PktEntityEventParity.java (drives the private FriendlyByteBuf
# decode ctor via reflection to pin arbitrary (entityId, eventId), encodes through the
# REAL STREAM_CODEC). Reuses the certified PacketBuffer (FriendlyByteBuf port); links
# its closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_entity_event_parity
    network/protocol/game/PktEntityEventParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_entity_event_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_entity_event_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_entity_event_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_entity_event_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundGameEventPacket
# ClientboundGameEventPacket StreamCodec parity vs the REAL net.minecraft codec.
# Body == FriendlyByteBuf.writeByte(event.id) + writeFloat(param):
#   1 byte (Type id, 0..13) then 4 bytes big-endian IEEE-754 float = 5 bytes total.
#   read == readUnsignedByte() (0..255) then readFloat().
# Plain numbers only; no registry/ItemStack/Component/Holder/NBT. Reuses the certified
# PacketBuffer (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for
# writeNbt symbols) + miniz, bcrypt on WIN32. GT: tools/PktGameEventParity.java.
add_executable(pkt_game_event_parity
    network/protocol/game/PktGameEventParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_game_event_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_game_event_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_game_event_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_game_event_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Byte-exact parity for net.minecraft.network.protocol.login.ClientboundHelloPacket.
# Codec write order (verbatim from 26.1.2/src): writeUtf(serverId) + writeByteArray(publicKey)
# + writeByteArray(challenge) + writeBoolean(shouldAuthenticate), byte-exact + read round trip.
# Ground truth: tools/PktHelloCbParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_hello_cb_parity
    network/protocol/login/PktHelloCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_hello_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_hello_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_hello_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_hello_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.login.ServerboundHelloPacket
add_executable(pkt_hello_sb_parity
    network/protocol/login/PktHelloSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_hello_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_hello_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_hello_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_hello_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundHurtAnimationPacket
# Byte-exact parity for ClientboundHurtAnimationPacket vs the REAL STREAM_CODEC:
# record(int id, float yaw) -> writeVarInt(id) + writeFloat(yaw) (big-endian IEEE-754),
# byte-exact + read round trip. No registry/ItemStack/Component/Holder/NBT, so it
# exercises the certified PacketBuffer (FriendlyByteBuf port) directly. Ground truth:
# tools/PktHurtAnimationParity.java. Links the PacketBuffer closure (NbtIo/Tag pulled
# in for writeNbt symbols) + miniz, and bcrypt on WIN32.
add_executable(pkt_hurt_animation_parity
    network/protocol/game/PktHurtAnimationParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_hurt_animation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_hurt_animation_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_hurt_animation_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_hurt_animation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.login.ServerboundKeyPacket
# Codec: writeByteArray(keybytes) + writeByteArray(encryptedChallenge)
# (each = VarInt(len) + raw bytes). Ground truth: tools/PktKeySbParity.java.
add_executable(pkt_key_sb_parity
    network/protocol/login/PktKeySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_key_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_key_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_key_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_key_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.login.ServerboundLoginAcknowledgedPacket
# Byte-exact packet codec parity vs the REAL net.minecraft STREAM_CODEC.
# STREAM_CODEC = StreamCodec.unit(INSTANCE): an EMPTY body â€” encodes to zero
# bytes, decodes by returning the singleton without consuming any bytes.
add_executable(pkt_login_ack_sb_parity
    network/protocol/login/PktLoginAckSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_login_ack_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_login_ack_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_login_ack_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_login_ack_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.login.ClientboundLoginCompressionPacket
# Packet codec byte parity vs the REAL STREAM_CODEC. Body is a single VarInt
# (FriendlyByteBuf.writeVarInt(compressionThreshold), LEB128); the C++ PacketBuffer
# (writeVarInt/readVarInt) must reproduce the bytes exactly and round-trip-decode
# them. Ground truth: tools/PktLoginCompressionCbParity.java.
add_executable(pkt_login_compression_cb_parity
    network/protocol/login/PktLoginCompressionCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_login_compression_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_login_compression_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_login_compression_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_login_compression_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundMoveVehiclePacket
# ServerboundMoveVehiclePacket StreamCodec parity vs the REAL net.minecraft codec.
# STREAM_CODEC = StreamCodec.composite(Vec3.STREAM_CODEC, ..., FLOAT, ..., FLOAT, ..., BOOL, ...).
# Body == writeDouble(x); writeDouble(y); writeDouble(z) [Vec3] + writeFloat(yRot) +
# writeFloat(xRot) + writeBoolean(onGround) -> always 33 bytes, byte-exact + read
# round-trip. GT: tools/PktMoveVehicleSbParity.java. Reuses the certified PacketBuffer
# (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for writeNbt
# symbols) + miniz, and bcrypt on WIN32.
add_executable(pkt_move_vehicle_sb_parity
    network/protocol/game/PktMoveVehicleSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_vehicle_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_vehicle_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_vehicle_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_vehicle_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundOpenSignEditorPacket
# Byte-exact parity for ClientboundOpenSignEditorPacket vs the REAL STREAM_CODEC:
# writeBlockPos(pos) == writeLong(pos.asLong()) + writeBoolean(isFrontText).
# Verifies the certified PacketBuffer (FriendlyByteBuf port) directly; round-trips
# the body back to (x,y,z,isFrontText). Ground truth: tools/PktOpenSignEditorParity.java.
# Links the PacketBuffer closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz,
# bcrypt on WIN32.
add_executable(pkt_open_sign_editor_parity
    network/protocol/game/PktOpenSignEditorParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_open_sign_editor_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_open_sign_editor_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_open_sign_editor_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_open_sign_editor_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.ping.ServerboundPingRequestPacket
# Byte-exact codec parity vs the REAL STREAM_CODEC. The packet body is a single
# fixed-width big-endian long (ByteBuf.writeLong(time) == FriendlyByteBuf.writeLong).
# The C++ side exercises the certified mc::net::PacketBuffer directly (no packet
# header). Ground truth: tools/PktPingRequestSbParity.java.
add_executable(pkt_ping_request_sb_parity
    network/protocol/ping/PktPingRequestSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_ping_request_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_ping_request_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_ping_request_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_ping_request_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.ping.ClientboundPongResponsePacket
# ClientboundPongResponsePacket codec parity vs the REAL net.minecraft StreamCodec.
# `public record ClientboundPongResponsePacket(long time)` â€” body is exactly
# FriendlyByteBuf.writeLong(time) -> 8-byte big-endian long (read side readLong()).
# The C++ PacketBuffer must reproduce the bytes exactly and round-trip-decode them.
# Ground truth: tools/PktPongResponseCbParity.java.
add_executable(pkt_pong_response_cb_parity
    network/protocol/ping/PktPongResponseCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_pong_response_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_pong_response_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_pong_response_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_pong_response_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundRotateHeadPacket
# Byte-exact parity for ClientboundRotateHeadPacket vs the real STREAM_CODEC.
# Body == FriendlyByteBuf.writeVarInt(entityId) + writeByte(yHeadRot) (a SIGNED byte,
# read back via readByte()). No id/length prefix (Packet.codec -> StreamCodec.ofMember).
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktRotateHeadParity.java.
add_executable(pkt_rotate_head_parity
    network/protocol/game/PktRotateHeadParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_rotate_head_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_rotate_head_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_rotate_head_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_rotate_head_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetCameraPacket
# ClientboundSetCameraPacket StreamCodec parity vs the REAL net.minecraft codec.
# Body is exactly FriendlyByteBuf.writeVarInt(cameraId)/readVarInt() (plain LEB128,
# NOT zig-zag; Packet.codec -> StreamCodec.ofMember, no packet-id prefix). Verifies
# the certified PacketBuffer (FriendlyByteBuf port) directly. Ground truth:
# tools/PktSetCameraParity.java.
add_executable(pkt_set_camera_parity
    network/protocol/game/PktSetCameraParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_camera_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_camera_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_camera_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_camera_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetChunkCacheCenterPacket
# Byte-exact parity for ClientboundSetChunkCacheCenterPacket vs the real STREAM_CODEC.
# Body == FriendlyByteBuf.writeVarInt(x); writeVarInt(z) (LEB128, negatives -> 5 bytes;
# Packet.codec, no id prefix). Reuses the certified PacketBuffer (FriendlyByteBuf port).
# Ground truth: tools/PktSetChunkCenterParity.java.
add_executable(pkt_set_chunk_center_parity
    network/protocol/game/PktSetChunkCenterParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_chunk_center_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_chunk_center_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_chunk_center_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_chunk_center_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetChunkCacheRadiusPacket
# Byte-exact parity for ClientboundSetChunkCacheRadiusPacket vs the real STREAM_CODEC.
# Body is exactly FriendlyByteBuf.writeVarInt(radius)/readVarInt() (a single bare signed
# VarInt, no zig-zag, no packet-id prefix). Reuses the certified PacketBuffer
# (FriendlyByteBuf port) directly. Ground truth: tools/PktSetChunkRadiusParity.java.
add_executable(pkt_set_chunk_radius_parity
    network/protocol/game/PktSetChunkRadiusParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_chunk_radius_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_chunk_radius_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_chunk_radius_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_chunk_radius_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetEntityMotionPacket
# 26.1.2 STREAM_CODEC = composite(ByteBufCodecs.VAR_INT ::id, Vec3.LP_STREAM_CODEC ::movement, ::new)
# => VarInt(id) + LpVec3-quantized Vec3 (net.minecraft.network.LpVec3.write). The legacy
# `short xa/ya/za` form is GONE in this version. The test reproduces LpVec3.write (helpers
# byte-identical to the certified mcpp/src/network/LpVec3.h) and writes through PacketBuffer's
# writeVarInt/writeByte/writeInt/writeVarInt, requiring byte+count parity vs the REAL codec.
# GT: tools/PktSetEntityMotionParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its closure (NbtIo/Tag for writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_set_entity_motion_parity
    network/protocol/game/PktSetEntityMotionParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_entity_motion_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_entity_motion_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_entity_motion_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_entity_motion_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetExperiencePacket
# Byte-exact parity for ClientboundSetExperiencePacket vs the REAL STREAM_CODEC.
# Body is exactly: writeFloat(experienceProgress) ++ writeVarInt(experienceLevel)
# ++ writeVarInt(totalExperience) -- NOTE the on-wire order is progress, LEVEL, TOTAL
# (the ctor arg order is total before level, but write() emits level first). Verifies
# the certified PacketBuffer (FriendlyByteBuf port) directly. No new header needed.
# Ground truth: tools/PktSetExperienceParity.java.
add_executable(pkt_set_experience_parity
    network/protocol/game/PktSetExperienceParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_experience_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_experience_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_experience_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_experience_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetHealthPacket
# Byte-exact parity for ClientboundSetHealthPacket vs the REAL STREAM_CODEC.
# Body == FriendlyByteBuf.writeFloat(health) + writeVarInt(food) + writeFloat(saturation)
# (real 26.1.2 source lines 22-32; Packet.codec -> StreamCodec.ofMember, no id prefix).
# Reuses the certified PacketBuffer (FriendlyByteBuf port) directly; links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz (+ bcrypt on WIN32).
# Ground truth: tools/PktSetHealthParity.java.
add_executable(pkt_set_health_parity
    network/protocol/game/PktSetHealthParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_health_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_health_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_health_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_health_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetHeldSlotPacket
# Byte-exact parity for ClientboundSetHeldSlotPacket vs the real STREAM_CODEC.
# The packet is a record(int slot) encoded via ByteBufCodecs.VAR_INT, so the body
# is exactly ONE VarInt(slot). Verifies the certified PacketBuffer (FriendlyByteBuf
# port) writeVarInt/readVarInt directly. Ground truth: tools/PktSetHeldSlotCbParity.java.
add_executable(pkt_set_held_slot_cb_parity
    network/protocol/game/PktSetHeldSlotCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_held_slot_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_held_slot_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_held_slot_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_held_slot_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.status.ServerboundStatusRequestPacket
# ServerboundStatusRequestPacket StreamCodec parity vs the REAL net.minecraft codec.
# STREAM_CODEC == StreamCodec.unit(INSTANCE): a unit codec whose encode writes NOTHING
# and whose decode returns the singleton consuming no bytes. The packet has no fields
# on the wire, so the encoded body is exactly zero bytes (readableBytes()==0). The test
# verifies the certified PacketBuffer (FriendlyByteBuf port) produces an empty body and
# that the empty Java bytes decode to an empty/exhausted buffer. Ground truth:
# tools/PktStatusRequestSbParity.java.
add_executable(pkt_status_request_sb_parity
    network/protocol/status/PktStatusRequestSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_status_request_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_status_request_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_status_request_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_status_request_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ClientboundStoreCookiePacket byte-exact codec parity vs
# tools/PktStoreCookieCbParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_store_cookie_cb_parity
    network/protocol/common/PktStoreCookieCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_store_cookie_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_store_cookie_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_store_cookie_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_store_cookie_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundTakeItemEntityPacket
# Byte-exact parity for ClientboundTakeItemEntityPacket vs the REAL STREAM_CODEC.
# Body is exactly three VarInts in order: writeVarInt(itemId), writeVarInt(playerId),
# writeVarInt(amount) (read: readVarInt x3). Packet.codec -> no packet-id prefix.
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktTakeItemEntityParity.java.
add_executable(pkt_take_item_entity_parity
    network/protocol/game/PktTakeItemEntityParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_take_item_entity_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_take_item_entity_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_take_item_entity_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_take_item_entity_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== Parallel-authored packet-codec gates (workflow wave 8, netcode play/movement) =====

# net.minecraft.network.protocol.game.ServerboundAcceptTeleportationPacket
# Byte-exact parity for the ServerboundAcceptTeleportationPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundAcceptTeleportationPacket.java:9-24):
#   write : FriendlyByteBuf.writeVarInt(this.id)
#   read  : input.readVarInt()
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix, so the whole wire
# payload is a single VarInt (LEB128) of the signed int `id`. Reuses the certified
# PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktAcceptTeleportSbParity.java.
add_executable(pkt_accept_teleport_sb_parity
    network/protocol/game/PktAcceptTeleportSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_accept_teleport_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_accept_teleport_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_accept_teleport_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_accept_teleport_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundChangeDifficultyPacket
# Byte-exact packet codec parity vs the REAL net.minecraft STREAM_CODEC.
# Body = StreamCodec.composite(Difficulty.STREAM_CODEC, ByteBufCodecs.BOOL):
#   Difficulty.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Difficulty::getId)
#     -> VarInt(getId())  (NOT writeEnum(ordinal), NOT a raw fixed byte)
#   ByteBufCodecs.BOOL    -> single 0/1 byte
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktChangeDifficultyParity.java.
add_executable(pkt_change_difficulty_parity
    network/protocol/game/PktChangeDifficultyParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_change_difficulty_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_change_difficulty_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_change_difficulty_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_change_difficulty_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundClearTitlesPacket
# Byte-exact parity for the ClientboundClearTitlesPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ClientboundClearTitlesPacket.java:9-24):
#   write : FriendlyByteBuf.writeBoolean(resetTimes)  -> single byte 0x01/0x00
#   read  : input.readBoolean()
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix. Reuses the
# certified PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktClearTitlesParity.java.
add_executable(pkt_clear_titles_parity
    network/protocol/game/PktClearTitlesParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_clear_titles_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_clear_titles_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_clear_titles_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_clear_titles_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ServerboundClientInformationPacket
# Byte-exact parity for the ClientInformation record codec vs the REAL STREAM_CODEC:
#   writeUtf(language,16) + writeByte(viewDistance) + writeEnum(chatVisibility=VarInt ordinal)
#   + writeBoolean(chatColors) + writeByte(modelCustomisation) + writeEnum(mainHand=VarInt ordinal)
#   + writeBoolean(textFilteringEnabled) + writeBoolean(allowsListing) + writeEnum(particleStatus=VarInt ordinal).
add_executable(pkt_client_information_sb_parity
    network/protocol/common/PktClientInformationSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_client_information_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_client_information_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_client_information_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_client_information_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundContainerClosePacket
# Single `int containerId`; STREAM_CODEC body == FriendlyByteBuf.writeContainerId ==
# VarInt.write (no zig-zag), no packet-id prefix. Reuses the certified
# mc::net::PacketBuffer (writeVarInt/readVarInt). Ground truth:
# tools/PktContainerCloseCbParity.java. Links the PacketBuffer closure
# (NbtIo/Tag pulled in for writeNbt symbols) + miniz; bcrypt on WIN32.
add_executable(pkt_container_close_cb_parity
    network/protocol/game/PktContainerCloseCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_close_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_close_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_close_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_close_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundCooldownPacket
# Byte-exact parity vs the REAL ClientboundCooldownPacket.STREAM_CODEC
# (StreamCodec.composite). Codec body, in order:
#   (1) Identifier.STREAM_CODEC = STRING_UTF8.map(parse,toString) -> writeUtf of
#       Identifier.toString() ("namespace:path"): VarInt(UTF-8 byte len)+bytes, max 32767.
#   (2) ByteBufCodecs.VAR_INT -> writeVarInt(duration) (LEB128).
# No registry-held type on the wire; reuses the certified PacketBuffer (FriendlyByteBuf
# port: writeString==writeUtf, writeVarInt). Links its full closure (NbtIo/Tag for
# writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktCooldownParity.java.
add_executable(pkt_cooldown_parity
    network/protocol/game/PktCooldownParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_cooldown_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_cooldown_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_cooldown_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_cooldown_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# Byte-exact parity gate for ClientboundEntityPositionSyncPacket vs its REAL STREAM_CODEC.
# Wire: VarInt(id) + PositionMoveRotation{Vec3 position(3xdouble BE), Vec3 deltaMovement(3xdouble BE),
# float yRot(BE), float xRot(BE)} + bool onGround. Exercises mc::net::PacketBuffer
# (writeVarInt/writeDouble/writeFloat/writeBool), requiring byte+count parity vs the REAL codec.
# GT: tools/PktEntityPositionSyncParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its closure (NbtIo/Tag for writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_entity_position_sync_parity
    network/protocol/game/PktEntityPositionSyncParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_entity_position_sync_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_entity_position_sync_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_entity_position_sync_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_entity_position_sync_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundInitializeBorderPacket
# Byte-exact parity for ClientboundInitializeBorderPacket vs the REAL STREAM_CODEC
# (Packet.codec(::write, ::new)). Wire body is exactly:
#   writeDouble(newCenterX) ++ writeDouble(newCenterZ) ++ writeDouble(oldSize)
#   ++ writeDouble(newSize) ++ writeVarLong(lerpTime) ++ writeVarInt(newAbsoluteMaxSize)
#   ++ writeVarInt(warningBlocks) ++ writeVarInt(warningTime).
# All primitives -> reuses the certified PacketBuffer (FriendlyByteBuf port) directly.
# No new header. Ground truth: tools/PktInitializeBorderParity.java.
add_executable(pkt_initialize_border_parity
    network/protocol/game/PktInitializeBorderParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_initialize_border_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_initialize_border_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_initialize_border_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_initialize_border_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundLevelEventPacket
# Byte-exact parity vs the REAL ClientboundLevelEventPacket.STREAM_CODEC
# (Packet.codec over a plain FriendlyByteBuf). Codec body, VERBATIM
# (ClientboundLevelEventPacket.java:32-37): writeInt(type); writeBlockPos(pos)
# == writeLong(pos.asLong()); writeInt(data); writeBoolean(globalEvent).
# All-primitive wire (int/long-packed BlockPos/int/bool) -- no registry/ItemStack/
# Component/Holder/NBT -- so the certified PacketBuffer (FriendlyByteBuf port)
# rebuilds the body directly. Links PacketBuffer's full closure (NbtIo/Tag for
# writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktLevelEventParity.java.
add_executable(pkt_level_event_parity
    network/protocol/game/PktLevelEventParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_level_event_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_level_event_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_level_event_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_level_event_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundLowDiskSpaceWarningPacket byte-exact codec
# parity vs tools/PktLowDiskSpaceParity.java. STREAM_CODEC == StreamCodec.unit(INSTANCE):
# a zero-byte unit packet (no fields on the wire). Reuses the certified PacketBuffer
# (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols)
# + miniz (+ bcrypt on Windows).
add_executable(pkt_low_disk_space_parity
    network/protocol/game/PktLowDiskSpaceParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_low_disk_space_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_low_disk_space_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_low_disk_space_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_low_disk_space_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Pos
# Byte-exact parity for the nested Pos subtype vs the REAL Pos.STREAM_CODEC
# (Packet.codec(Pos::write, Pos::read)). Body is exactly:
#   writeVarInt(entityId) ++ writeShort(xa) ++ writeShort(ya) ++ writeShort(za) ++ writeBoolean(onGround)
# (plain LEB128 VarInt + three BE signed shorts + one bool byte; StreamCodec.ofMember,
# no packet-id / length prefix). Verifies the certified PacketBuffer (FriendlyByteBuf port)
# directly; no new per-packet header. Ground truth: tools/PktMoveEntityPosParity.java.
add_executable(pkt_move_entity_pos_parity
    network/protocol/game/PktMoveEntityPosParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_entity_pos_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_entity_pos_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_entity_pos_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_entity_pos_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.PosRot
# Byte-exact parity for ClientboundMoveEntityPacket.PosRot vs the REAL PosRot.STREAM_CODEC.
# Body is exactly (PosRot::write order): writeVarInt(entityId) ++ writeShort(xa) ++ writeShort(ya)
# ++ writeShort(za) ++ writeByte(yRot) ++ writeByte(xRot) ++ writeBoolean(onGround). All primitives;
# verifies the certified PacketBuffer (FriendlyByteBuf port) directly. No new header needed.
# Ground truth: tools/PktMoveEntityPosRotParity.java.
add_executable(pkt_move_entity_posrot_parity
    network/protocol/game/PktMoveEntityPosRotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_entity_posrot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_entity_posrot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_entity_posrot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_entity_posrot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundMoveEntityPacket.Rot
# ClientboundMoveEntityPacket.Rot StreamCodec parity vs the REAL net.minecraft codec.
# Body is exactly FriendlyByteBuf writeVarInt(entityId)+writeByte(yRot)+writeByte(xRot)+
# writeBoolean(onGround) (plain LEB128 VarInt + two signed bytes + 0/1 bool; Packet.codec
# -> StreamCodec.ofMember, no packet-id prefix). Verifies the certified PacketBuffer
# (FriendlyByteBuf port) directly. Ground truth: tools/PktMoveEntityRotParity.java.
add_executable(pkt_move_entity_rot_parity
    network/protocol/game/PktMoveEntityRotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_entity_rot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_entity_rot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_entity_rot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_entity_rot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Pos
# Pos codec parity vs the REAL STREAM_CODEC (Packet.codec(Pos::write, Pos::read)):
# writeDouble(x) + writeDouble(y) + writeDouble(z) + writeByte(packFlags(onGround,
# horizontalCollision)) -- byte-exact + read round trip. Ground truth:
# tools/PktMovePlayerPosParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols).
add_executable(pkt_move_player_pos_parity
    network/protocol/game/PktMovePlayerPosParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_player_pos_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_player_pos_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_player_pos_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_player_pos_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.PosRot STREAM_CODEC parity.
# Body = double(x)++double(y)++double(z)++float(yRot)++float(xRot)++byte(packFlags), 25 bytes,
# no packet-id prefix (Packet.codec == StreamCodec.ofMember). All primitives -> exercises the
# certified PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktMovePlayerPosRotParity.java.
add_executable(pkt_move_player_posrot_parity
    network/protocol/game/PktMovePlayerPosRotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_player_posrot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_player_posrot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_player_posrot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_player_posrot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Rot
add_executable(pkt_move_player_rot_parity
    network/protocol/game/PktMovePlayerRotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_player_rot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_player_rot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_player_rot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_player_rot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundOpenBookPacket
# ClientboundOpenBookPacket codec parity vs the REAL STREAM_CODEC: the whole body is
# FriendlyByteBuf.writeEnum(hand) == writeVarInt(hand.ordinal()) and readEnum on
# decode (InteractionHand: MAIN_HAND ordinal 0 / OFF_HAND ordinal 1 -> 0x00 / 0x01).
# Byte-exact + readableBytes + PacketBuffer round-trip. Ground truth:
# tools/PktOpenBookParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf
# port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_open_book_parity
    network/protocol/game/PktOpenBookParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_open_book_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_open_book_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_open_book_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_open_book_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundPlayerAbilitiesPacket
# Byte-exact parity for ClientboundPlayerAbilitiesPacket vs the real STREAM_CODEC.
# The packet body is exactly: writeByte(bitfield) + writeFloat(flyingSpeed) +
# writeFloat(walkingSpeed), where bitfield packs four booleans
# (invulnerable=1, isFlying=2, canFly=4, instabuild=8). Wire payload = 1B + 4B + 4B = 9B.
# Verifies the certified PacketBuffer (FriendlyByteBuf port) writeByte/readByte (raw byte)
# and writeFloat/readFloat (raw int bits big-endian) directly. No packet-id prefix
# (StreamCodec.ofMember). Ground truth: tools/PktPlayerAbilitiesParity.java.
add_executable(pkt_player_abilities_parity
    network/protocol/game/PktPlayerAbilitiesParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_abilities_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_abilities_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_abilities_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_abilities_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundPlayerCombatEndPacket
# Byte-exact parity for the ClientboundPlayerCombatEndPacket STREAM_CODEC vs the REAL
# net.minecraft codec. The packet holds a single `private final int duration`; body
# (ClientboundPlayerCombatEndPacket.java:23-29):
#   write : output.writeVarInt(this.duration)
#   read  : input.readVarInt()
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix, so the whole wire
# payload is one VarInt (LEB128, NO zig-zag -> negatives are 5 bytes). Reuses the certified
# PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktPlayerCombatEndParity.java.
add_executable(pkt_player_combat_end_parity
    network/protocol/game/PktPlayerCombatEndParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_combat_end_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_combat_end_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_combat_end_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_combat_end_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundPlayerCombatEnterPacket byte-exact
# codec parity vs tools/PktPlayerCombatEnterParity.java. The packet is the singleton
# INSTANCE with STREAM_CODEC = StreamCodec.unit(INSTANCE): encode writes ZERO bytes
# (only asserts the singleton) and decode returns the singleton consuming nothing.
# The on-wire body is exactly empty â€” no fields, nothing for PacketBuffer to build.
# Reuses the certified mc::net::PacketBuffer (FriendlyByteBuf port); links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz; bcrypt on WIN32.
add_executable(pkt_player_combat_enter_parity
    network/protocol/game/PktPlayerCombatEnterParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_combat_enter_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_combat_enter_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_combat_enter_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_combat_enter_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundPlayerInputPacket
# ServerboundPlayerInputPacket: byte-exact packet codec parity vs the REAL
# net.minecraft STREAM_CODEC. The packet is a record wrapping one Input; its codec is
# StreamCodec.composite(Input.STREAM_CODEC, ::input, ::new), so the wire payload is
# exactly Input.STREAM_CODEC: a SINGLE byte packing 7 booleans
# (forward=1, backward=2, left=4, right=8, jump=16, shift=32, sprint=64; bit 0x80 unused).
# Verified against 26.1.2/src Input.java lines 7-38 + ServerboundPlayerInputPacket.java
# lines 9-12. Reuses the certified PacketBuffer (FriendlyByteBuf port); links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz (+ bcrypt on WIN32).
add_executable(pkt_player_input_sb_parity
    network/protocol/game/PktPlayerInputSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_input_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_input_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_input_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_input_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundPlayerLookAtPacket
# Byte-exact parity vs the real packet write(FriendlyByteBuf) / new(FriendlyByteBuf).
# Body == writeEnum(fromAnchor)==writeVarInt(ordinal); writeDouble x,y,z (BE);
# writeBoolean(atEntity); if atEntity: writeVarInt(entity); writeEnum(toAnchor)==writeVarInt(ordinal).
# EntityAnchorArgument.Anchor: FEET=0, EYES=1. No id prefix (Packet.codec). Reuses the certified
# PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktPlayerLookAtParity.java.
add_executable(pkt_player_look_at_parity
    network/protocol/game/PktPlayerLookAtParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_look_at_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_look_at_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_look_at_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_look_at_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundPlayerRotationPacket
# Byte-exact parity for ClientboundPlayerRotationPacket vs the REAL STREAM_CODEC.
# Record(float yRot, boolean relativeY, float xRot, boolean relativeX); STREAM_CODEC is
# StreamCodec.composite(FLOAT yRot, BOOL relativeY, FLOAT xRot, BOOL relativeX). Body is
# exactly: writeFloat(yRot) ++ writeBool(relativeY) ++ writeFloat(xRot) ++ writeBool(relativeX),
# 10 bytes, no packet-id prefix. All primitives -> exercises the certified PacketBuffer
# (FriendlyByteBuf port) directly. Ground truth: tools/PktPlayerRotationParity.java.
add_executable(pkt_player_rotation_parity
    network/protocol/game/PktPlayerRotationParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_rotation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_rotation_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_rotation_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_rotation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.common.ServerboundPongPacket
# Byte-exact parity for the ServerboundPongPacket STREAM_CODEC vs the REAL net.minecraft
# codec. Body (ServerboundPongPacket.java:16-22): write = writeInt(id); read = readInt().
# Single field `int id` -> a plain 4-byte big-endian signed int (FriendlyByteBuf.writeInt;
# no VarInt, no zig-zag). Packet.codec -> StreamCodec.ofMember: body only, no id/length
# prefix. Reuses the certified PacketBuffer (FriendlyByteBuf port). Ground truth:
# tools/PktPongGameSbParity.java. (Class lives in protocol.common in 26.1.2; hint said game.)
add_executable(pkt_pong_game_sb_parity
    network/protocol/game/PktPongGameSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_pong_game_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_pong_game_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_pong_game_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_pong_game_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundProjectilePowerPacket
# Byte-exact parity for ClientboundProjectilePowerPacket vs the REAL STREAM_CODEC
# (Packet.codec(write, new)): write() does writeVarInt(id) then writeDouble(accelerationPower),
# decode reads readVarInt() then readDouble() in the same order. Both fields are plain
# primitives, so this verifies the certified PacketBuffer (FriendlyByteBuf port) directly:
# VarInt LEB128 + big-endian IEEE-754 double. Round-trips the body back to (id, power).
# Ground truth: tools/PktProjectilePowerParity.java. Links the PacketBuffer closure
# (NbtIo/Tag pulled in for writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_projectile_power_parity
    network/protocol/game/PktProjectilePowerParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_projectile_power_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_projectile_power_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_projectile_power_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_projectile_power_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundRemoveEntitiesPacket
# Byte-exact parity for ClientboundRemoveEntitiesPacket vs the real STREAM_CODEC.
# Body = FriendlyByteBuf.writeIntIdList(entityIds): writeVarInt(size) + each id VarInt.
# Ground truth: tools/PktRemoveEntitiesParity.java (encode/decode through the real codec).
add_executable(pkt_remove_entities_parity
    network/protocol/game/PktRemoveEntitiesParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_remove_entities_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_remove_entities_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_remove_entities_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_remove_entities_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundResetScorePacket
# Byte-exact parity vs the REAL ClientboundResetScorePacket.STREAM_CODEC. Codec body is
# writeUtf(owner); writeNullable(objectiveName, writeUtf)  [= writeBoolean(present) then
# writeUtf if present]. Read side: readUtf(); readNullable(readUtf). Reuses the certified
# PacketBuffer (FriendlyByteBuf port): writeString==writeUtf, writeBool==writeBoolean.
# Links its full closure (NbtIo/Tag for writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktResetScoreParity.java.
add_executable(pkt_reset_score_parity
    network/protocol/game/PktResetScoreParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_reset_score_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_reset_score_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_reset_score_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_reset_score_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== Parallel-authored packet-codec gates (workflow wave 9, netcode play) =====

# net.minecraft.network.protocol.game.ServerboundBlockEntityTagQueryPacket
# Byte-exact parity vs the REAL ServerboundBlockEntityTagQueryPacket.STREAM_CODEC.
# Codec body (write(FriendlyByteBuf), lines 26-29) is, VERBATIM:
#   writeVarInt(transactionId);  writeBlockPos(pos) == writeLong(pos.asLong())
# No registry/Holder/ItemStack/Component/NBT on the wire, so the certified
# PacketBuffer (the FriendlyByteBuf port) rebuilds the body directly. Links its
# full closure (NbtIo/Tag for writeNbt symbols) + miniz, and bcrypt on WIN32.
add_executable(pkt_block_entity_tag_query_sb_parity
    network/protocol/game/PktBlockEntityTagQuerySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_block_entity_tag_query_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_block_entity_tag_query_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_block_entity_tag_query_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_block_entity_tag_query_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundChangeDifficultyPacket
# Byte-exact parity for the ServerboundChangeDifficultyPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundChangeDifficultyPacket.java:9-12):
#   STREAM_CODEC = StreamCodec.composite(Difficulty.STREAM_CODEC, ::difficulty, ::new)
#   -> exactly one field: VarInt(difficulty.getId()) via ByteBufCodecs.idMapper
#      (ByteBufCodecs.idMapper lines 542-553 write VarInt.write(getId())).
#      getId(): PEACEFUL=0/EASY=1/NORMAL=2/HARD=3. No boolean, no trailing data
#      (unlike the clientbound variant). Reuses the certified PacketBuffer
#      (FriendlyByteBuf port). Ground truth: tools/PktChangeDifficultySbParity.java.
add_executable(pkt_change_difficulty_sb_parity
    network/protocol/game/PktChangeDifficultySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_change_difficulty_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_change_difficulty_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_change_difficulty_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_change_difficulty_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundChangeGameModePacket
# Body is a single GameType encoded via ByteBufCodecs.idMapper -> VarInt(getId()).
add_executable(pkt_change_gamemode_sb_parity
    network/protocol/game/PktChangeGameModeSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_change_gamemode_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_change_gamemode_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_change_gamemode_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_change_gamemode_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundChunkBatchReceivedPacket
# Byte-exact parity for ServerboundChunkBatchReceivedPacket vs the real STREAM_CODEC.
# Record = (float desiredChunksPerTick); write(FriendlyByteBuf) is a single
# writeFloat(desiredChunksPerTick) -> big-endian IEEE-754 4-byte float; decode = readFloat().
# Packet.codec -> no packet-id prefix, just the body.
# Ground truth: tools/PktChunkBatchReceivedSbParity.java (encode/decode through the real codec).
add_executable(pkt_chunk_batch_received_sb_parity
    network/protocol/game/PktChunkBatchReceivedSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_chunk_batch_received_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_chunk_batch_received_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_chunk_batch_received_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_chunk_batch_received_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundClientTickEndPacket
# Byte-exact parity for ServerboundClientTickEndPacket vs the REAL STREAM_CODEC.
# This is a UNIT/EMPTY packet: STREAM_CODEC = StreamCodec.unit(INSTANCE), so encode() writes
# ZERO bytes and decode() reads ZERO bytes and returns the singleton. The wire payload is the
# empty byte string. The C++ side re-encodes via PacketBuffer (writes nothing), requires the
# produced bytes match the empty expected payload and readableBytes==0, then "decodes" the
# empty payload and requires the buffer is fully consumed. Verifies the certified PacketBuffer
# (FriendlyByteBuf port) handles the no-field unit case. Ground truth:
# tools/PktClientTickEndSbParity.java. Links the PacketBuffer closure (NbtIo/Tag pulled in for
# writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_client_tick_end_sb_parity
    network/protocol/game/PktClientTickEndSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_client_tick_end_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_client_tick_end_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_client_tick_end_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_client_tick_end_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundConfigurationAcknowledgedPacket byte-exact
# codec parity vs tools/PktConfigAckSbParity.java. STREAM_CODEC == StreamCodec.unit(INSTANCE):
# the encoder writes NOTHING and the decoder returns the singleton, so the on-wire body is
# exactly zero bytes. The C++ test mirrors that (empty PacketBuffer, hex "") and verifies the
# empty Java bytes decode to an empty/exhausted buffer. Reuses the certified PacketBuffer
# (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_config_ack_sb_parity
    network/protocol/game/PktConfigAckSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_config_ack_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_config_ack_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_config_ack_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_config_ack_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundEntityTagQueryPacket
# Byte-exact packet codec parity vs the REAL net.minecraft STREAM_CODEC.
# Body = Packet.codec(write, new); write(FriendlyByteBuf) in wire order:
#   writeVarInt(transactionId);  -> LEB128 VarInt of signed transactionId
#   writeVarInt(entityId);       -> LEB128 VarInt of signed entityId
# No packet-id prefix. writeVarInt does NOT zig-zag (negatives = 5 bytes).
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktEntityTagQuerySbParity.java.
add_executable(pkt_entity_tag_query_sb_parity
    network/protocol/game/PktEntityTagQuerySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_entity_tag_query_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_entity_tag_query_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_entity_tag_query_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_entity_tag_query_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundLockDifficultyPacket
# Byte-exact parity for the ServerboundLockDifficultyPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundLockDifficultyPacket.java:9-24):
#   write : FriendlyByteBuf.writeBoolean(locked)  -> single byte 0x01/0x00
#   read  : input.readBoolean()
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix. Reuses the
# certified PacketBuffer (FriendlyByteBuf port). Ground truth: tools/PktLockDifficultySbParity.java.
add_executable(pkt_lock_difficulty_sb_parity
    network/protocol/game/PktLockDifficultySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_lock_difficulty_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_lock_difficulty_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_lock_difficulty_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_lock_difficulty_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_paddle_boat_sb_parity
    network/protocol/game/PktPaddleBoatSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_paddle_boat_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_paddle_boat_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_paddle_boat_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_paddle_boat_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundPlayerAbilitiesPacket
# Byte-exact parity for the SERVERBOUND ServerboundPlayerAbilitiesPacket STREAM_CODEC vs
# the REAL net.minecraft codec. The packet body is exactly ONE byte: a bitfield that packs
# only isFlying (FLAG_FLYING = 2):
#   write : byte bitfield = 0; if (isFlying) bitfield |= 2; output.writeByte(bitfield)
#   read  : byte bitfield = input.readByte(); isFlying = (bitfield & 2) != 0
# Packet.codec -> StreamCodec.ofMember: no id/length prefix, just the 1-byte body
# (0x00 not-flying / 0x02 flying). Reuses the certified PacketBuffer (FriendlyByteBuf port)
# writeByte/readByte. The DEC battery covers EVERY input byte 0..255 for the (& 2) mask.
# Ground truth: tools/PktPlayerAbilitiesSbParity.java.
add_executable(pkt_player_abilities_sb_parity
    network/protocol/game/PktPlayerAbilitiesSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_abilities_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_abilities_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_abilities_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_abilities_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundPlayerActionPacket
# Byte-exact parity for ServerboundPlayerActionPacket vs the REAL STREAM_CODEC.
# Body (write(FriendlyByteBuf), ServerboundPlayerActionPacket.java:37-42):
#   writeEnum(action)            == writeVarInt(action.ordinal())  (Action: 0..7)
#   writeBlockPos(pos)           == writeLong(pos.asLong())         (BE 8-byte 26/12/26 long)
#   writeByte(direction.get3DDataValue())                          (one byte, 0..5)
#   writeVarInt(sequence)        == LEB128 VarInt
# All fields decompose to primitives the certified PacketBuffer (FriendlyByteBuf port)
# rebuilds directly. Ground truth: tools/PktPlayerActionSbParity.java.
add_executable(pkt_player_action_sb_parity
    network/protocol/game/PktPlayerActionSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_action_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_action_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_action_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_action_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundPlayerLoadedPacket byte-exact codec parity
# vs tools/PktPlayerLoadedSbParity.java. The packet's STREAM_CODEC is StreamCodec.unit(...):
# a zero-component record whose encoder writes NOTHING (zero-byte body). Reuses the certified
# PacketBuffer (FriendlyByteBuf port); links its full closure (NbtIo/Tag pulled in for
# writeNbt symbols) + miniz (+ bcrypt on WIN32).
add_executable(pkt_player_loaded_sb_parity
    network/protocol/game/PktPlayerLoadedSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_loaded_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_loaded_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_loaded_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_loaded_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSeenAdvancementsPacket
# Byte-exact packet codec parity vs the REAL net.minecraft STREAM_CODEC.
# STREAM_CODEC = Packet.codec(write, new) -> no packet-id prefix, just the body.
#   write: writeEnum(action) = writeVarInt(ordinal);
#          if action == OPENED_TAB: writeIdentifier(tab) = writeUtf(namespace:path)
#   (the tab is written ONLY for OPENED_TAB; there is NO leading boolean present-flag.)
# Verifies the certified PacketBuffer (FriendlyByteBuf port) writeVarInt (LEB128) and
# writeString (== writeUtf: VarInt byte-length + UTF-8 bytes) directly.
# Ground truth: tools/PktSeenAdvancementsSbParity.java.
add_executable(pkt_seen_advancements_sb_parity
    network/protocol/game/PktSeenAdvancementsSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_seen_advancements_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_seen_advancements_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_seen_advancements_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_seen_advancements_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSelectAdvancementsTabPacket
# Byte-exact parity for ClientboundSelectAdvancementsTabPacket vs the real STREAM_CODEC.
# The packet body is exactly: writeNullable(tab, writeIdentifier) ==
#   writeBoolean(present) + (if present) writeUtf(namespace:path).
# So wire = 0x00 (null tab) OR 0x01 + VarInt(utf8 len) + utf8 bytes (present tab).
# Reuses the certified PacketBuffer (FriendlyByteBuf port): writeBool (single byte)
# and writeString (== writeUtf: VarInt byte-length + UTF-8 bytes). Links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz. No packet-id prefix
# (StreamCodec via Packet.codec). Ground truth: tools/PktSelectAdvancementsTabParity.java.
add_executable(pkt_select_advancements_tab_parity
    network/protocol/game/PktSelectAdvancementsTabParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_select_advancements_tab_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_select_advancements_tab_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_select_advancements_tab_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_select_advancements_tab_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSelectBundleItemPacket
# Byte-exact parity for the ServerboundSelectBundleItemPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundSelectBundleItemPacket.java:20-23):
#   writeVarInt(slotId) + writeVarInt(selectedItemIndex).
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix. No zig-zag, so
# negatives encode as 5-byte LEB128. Reuses the certified PacketBuffer (FriendlyByteBuf
# port). Ground truth: tools/PktSelectBundleItemSbParity.java.
add_executable(pkt_select_bundle_item_sb_parity
    network/protocol/game/PktSelectBundleItemSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_select_bundle_item_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_select_bundle_item_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_select_bundle_item_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_select_bundle_item_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetBorderCenterPacket STREAM_CODEC parity.
# Body = double(newCenterX) ++ double(newCenterZ), 16 bytes, no packet-id prefix
# (Packet.codec body codec). Both fields plain doubles -> exercises the certified
# PacketBuffer (FriendlyByteBuf port) writeDouble/readDouble. Ground truth:
# tools/PktSetBorderCenterParity.java.
add_executable(pkt_set_border_center_parity
    network/protocol/game/PktSetBorderCenterParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_border_center_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_border_center_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_border_center_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_border_center_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket
# Byte-exact parity vs the REAL ClientboundSetBorderLerpSizePacket.STREAM_CODEC
# (Packet.codec over a plain FriendlyByteBuf). Codec body, VERBATIM
# (ClientboundSetBorderLerpSizePacket.java:29-33): writeDouble(oldSize);
# writeDouble(newSize); writeVarLong(lerpTime). All-primitive wire
# (double/double/VarLong) -- no registry/ItemStack/Component/Holder/NBT -- so the
# certified PacketBuffer (FriendlyByteBuf port) rebuilds the body directly. Links
# PacketBuffer's full closure (NbtIo/Tag for the writeNbt member it references).
add_executable(pkt_set_border_lerp_size_parity
    network/protocol/game/PktSetBorderLerpSizeParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_border_lerp_size_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_border_lerp_size_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_border_lerp_size_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_border_lerp_size_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetBorderSizePacket
# Byte-exact parity vs the REAL ClientboundSetBorderSizePacket.STREAM_CODEC
# (Packet.codec(::write, ::new) over a plain FriendlyByteBuf). Codec body, VERBATIM
# (ClientboundSetBorderSizePacket.java:23-25): writeDouble(size). Single primitive
# double (FLOAT64 big-endian) -- no registry/ItemStack/Component/Holder/NBT -- so the
# certified PacketBuffer (FriendlyByteBuf port) rebuilds the body directly. Links
# PacketBuffer's full closure (NbtIo/Tag) so the certified port compiles unchanged.
add_executable(pkt_set_border_size_parity
    network/protocol/game/PktSetBorderSizeParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_border_size_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_border_size_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_border_size_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_border_size_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_set_border_warn_delay_parity
    network/protocol/game/PktSetBorderWarnDelayParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_border_warn_delay_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_border_warn_delay_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_border_warn_delay_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_border_warn_delay_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetBorderWarningDistancePacket
# 26.1.2 STREAM_CODEC = Packet.codec(::write, ::new); write = FriendlyByteBuf.writeVarInt(this.warningBlocks),
# read = input.readVarInt(). Packet.codec -> StreamCodec.ofMember: no packet-id prefix, so the whole
# wire payload is a single signed-int VarInt (LEB128, no zig-zag; negatives -> 5 bytes). The test
# reproduces it via PacketBuffer.writeVarInt/readVarInt, requiring byte+count parity vs the REAL codec.
# GT: tools/PktSetBorderWarnDistParity.java. Reuses the certified PacketBuffer (FriendlyByteBuf port);
# links its closure (NbtIo/Tag for writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_set_border_warn_dist_parity
    network/protocol/game/PktSetBorderWarnDistParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_border_warn_dist_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_border_warn_dist_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_border_warn_dist_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_border_warn_dist_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetDefaultSpawnPositionPacket
# Byte-exact parity vs the REAL ClientboundSetDefaultSpawnPositionPacket.STREAM_CODEC.
# The packet wraps LevelData.RespawnData(GlobalPos globalPos, float yaw, float pitch).
# Wire body, in codec order:
#   writeUtf(dimensionId)   (Identifier.STREAM_CODEC -> STRING_UTF8: VarInt len + UTF-8 bytes)
#   writeLong(pos.asLong()) (BlockPos.STREAM_CODEC -> writeBlockPos, big-endian long)
#   writeFloat(yaw)         (ByteBufCodecs.FLOAT, big-endian rawIntBits)
#   writeFloat(pitch)       (ByteBufCodecs.FLOAT, big-endian rawIntBits)
# No Holder/ItemStack/Component/NBT/registry-id on the wire -- the dimension is a plain
# ResourceLocation string -- so the certified PacketBuffer (FriendlyByteBuf port) rebuilds it.
# Links PacketBuffer.cpp + its closure (NbtIo/Tag for writeNbt symbols) + miniz, bcrypt on WIN32.
# Ground truth: tools/PktSetDefaultSpawnParity.java.
add_executable(pkt_set_default_spawn_parity
    network/protocol/game/PktSetDefaultSpawnParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_default_spawn_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_default_spawn_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_default_spawn_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_default_spawn_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_set_display_objective_parity
    network/protocol/game/PktSetDisplayObjectiveParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_display_objective_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_display_objective_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_display_objective_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_display_objective_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_set_simulation_distance_parity
    network/protocol/game/PktSetSimulationDistanceParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_simulation_distance_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_simulation_distance_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_simulation_distance_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_simulation_distance_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetTitlesAnimationPacket
# Byte-exact parity for ClientboundSetTitlesAnimationPacket vs the REAL STREAM_CODEC:
# class(int fadeIn, int stay, int fadeOut) -> writeInt(fadeIn) + writeInt(stay) +
# writeInt(fadeOut) (big-endian 4-byte ints), byte-exact + read round trip. No
# registry/ItemStack/Component/Holder/NBT, so it exercises the certified PacketBuffer
# (FriendlyByteBuf port) directly. Ground truth: tools/PktSetTitlesAnimationParity.java.
# Links the PacketBuffer closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz,
# and bcrypt on WIN32.
add_executable(pkt_set_titles_animation_parity
    network/protocol/game/PktSetTitlesAnimationParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_titles_animation_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_titles_animation_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_titles_animation_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_titles_animation_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSignUpdatePacket
# Byte-exact codec parity vs the REAL STREAM_CODEC. Body is exactly:
#   writeBlockPos(pos)==writeLong(asLong) + writeBoolean(isFrontText) + 4x writeUtf(line).
# Reuses the certified mc::net::PacketBuffer (the FriendlyByteBuf port); no packet
# header on the wire. Ground truth: tools/PktSignUpdateSbParity.java.
add_executable(pkt_signedit_sb_parity
    network/protocol/game/PktSignUpdateSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_signedit_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_signedit_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_signedit_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_signedit_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundTeleportToEntityPacket
# Byte-exact parity for the ServerboundTeleportToEntityPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundTeleportToEntityPacket.java:13-28):
#   write : FriendlyByteBuf.writeUUID(this.uuid)
#   read  : input.readUUID()
# FriendlyByteBuf.writeUUID = writeLong(MSB) ++ writeLong(LSB), both big-endian 8-byte
# longs (FriendlyByteBuf.java:498-501). Packet.codec -> StreamCodec.ofMember: body only,
# no id/length prefix, so the whole wire payload is exactly 16 bytes = MSB(BE)++LSB(BE).
# Reuses the certified PacketBuffer (FriendlyByteBuf port). GT: tools/PktTeleportToEntitySbParity.java.
add_executable(pkt_spectate_entity_sb_parity
    network/protocol/game/PktTeleportToEntitySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_spectate_entity_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_spectate_entity_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_spectate_entity_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_spectate_entity_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundStopSoundPacket byte-exact codec
# parity vs tools/PktStopSoundParity.java. STREAM_CODEC = Packet.codec(write, ctor)
# (no packet-id prefix). write() branches on (source!=null, name!=null):
#   both  -> writeByte(3) + writeEnum(source)=writeVarInt(ordinal) + writeIdentifier(name)=writeUtf(toString)
#   src   -> writeByte(1) + writeEnum(source)
#   name  -> writeByte(2) + writeIdentifier(name)
#   none  -> writeByte(0)
# (ClientboundStopSoundPacket.java 40-56; FriendlyByteBuf.writeEnum 471-473 /
#  writeIdentifier 585-588 / writeUtf). Reuses the certified PacketBuffer
# (FriendlyByteBuf port); links its closure (NbtIo/Tag for writeNbt symbols) + miniz
# (+ bcrypt on Windows).
add_executable(pkt_stop_sound_parity
    network/protocol/game/PktStopSoundParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_stop_sound_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_stop_sound_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_stop_sound_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_stop_sound_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundUseItemOnPacket
# Byte-exact parity vs the REAL ServerboundUseItemOnPacket.STREAM_CODEC. Codec body
# (write(FriendlyByteBuf), ServerboundUseItemOnPacket.java:30-34) is:
#   writeEnum(hand) -> VarInt(hand.ordinal()) [MAIN_HAND=0, OFF_HAND=1];
#   writeBlockHitResult(blockHit) (FriendlyByteBuf.java:636-646):
#     writeBlockPos(pos) -> writeLong(asLong) BE long;
#     writeEnum(direction) -> VarInt(direction.ordinal()) [DOWN=0..EAST=5];
#     writeFloat(loc.x-pos.x()); writeFloat(loc.y-pos.y()); writeFloat(loc.z-pos.z()) BE;
#     writeBoolean(inside); writeBoolean(worldBorder);
#   writeVarInt(sequence).
# No Holder/ItemStack/Component/NBT on the wire -- all primitives. Reuses the certified
# PacketBuffer (FriendlyByteBuf port); links its full closure (NbtIo/Tag for writeNbt
# symbols) + miniz, and bcrypt on WIN32. Ground truth: tools/PktUseItemOnSbParity.java.
add_executable(pkt_use_item_on_sb_parity
    network/protocol/game/PktUseItemOnSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_use_item_on_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_use_item_on_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_use_item_on_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_use_item_on_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundUseItemPacket
# Byte-exact parity for the ServerboundUseItemPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ServerboundUseItemPacket.java:32-37 write / 25-30 read),
# IN ORDER: writeEnum(hand)==writeVarInt(hand.ordinal()); writeVarInt(sequence);
# writeFloat(yRot); writeFloat(xRot). InteractionHand MAIN_HAND=0/OFF_HAND=1
# (InteractionHand.java:10-13). All fields are primitives PacketBuffer supports
# (VarInt/VarInt/float/float). Packet.codec -> body only, no id/length prefix.
# Reuses the certified PacketBuffer (FriendlyByteBuf port); links its full closure
# (NbtIo/Tag pulled in for writeNbt symbols) + miniz (+ bcrypt on WIN32). Ground
# truth: tools/PktUseItemSbParity.java.
add_executable(pkt_use_item_sb_parity
    network/protocol/game/PktUseItemSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_use_item_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_use_item_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_use_item_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_use_item_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== Parallel-authored packet-codec gates (workflow wave 10, netcode config/game) =====

# net.minecraft.network.protocol.configuration.ServerboundAcceptCodeOfConductPacket
# byte-exact codec parity vs tools/PktAcceptCodeSbParity.java. The packet is a unit
# record whose STREAM_CODEC == StreamCodec.unit(INSTANCE): encode writes ZERO bytes and
# decode returns the singleton. The gate verifies the C++ PacketBuffer side likewise
# produces an empty body (size()==0, hex=="") and that the empty Java bytes decode to an
# empty/exhausted buffer. Reuses the certified PacketBuffer (FriendlyByteBuf port); links
# its full closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_accept_code_sb_parity
    network/protocol/configuration/PktAcceptCodeSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_accept_code_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_accept_code_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_accept_code_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_accept_code_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundBlockUpdatePacket
# Byte-exact parity vs the REAL ClientboundBlockUpdatePacket.STREAM_CODEC. The codec
# is StreamCodec.composite(BlockPos.STREAM_CODEC, ::getPos,
# ByteBufCodecs.idMapper(Block.BLOCK_STATE_REGISTRY), ::getBlockState, ::new), so the
# wire body is: writeBlockPos(pos) -> writeLong(pos.asLong()) (BE 8-byte long) then a
# plain VarInt of the BlockState registry id (BLOCK_STATE_REGISTRY.getIdOrThrow). No
# Holder/ResourceLocation/ItemStack/Component/NBT on the wire. Reuses the certified
# PacketBuffer (FriendlyByteBuf port); links its full closure (NbtIo/Tag for writeNbt
# symbols) + miniz, and bcrypt on WIN32. Ground truth: tools/PktBlockUpdateParity.java.
add_executable(pkt_block_upd_parity
    network/protocol/game/PktBlockUpdateParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_block_upd_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_block_upd_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_block_upd_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_block_upd_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundChatAckPacket
# Byte-exact parity for the ServerboundChatAckPacket STREAM_CODEC vs the REAL
# net.minecraft codec. The packet is a record(int offset); body is exactly
# (ServerboundChatAckPacket.java:8-19): writeVarInt(offset) / readVarInt().
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix, so the whole
# wire payload is one LEB128 VarInt of the signed int `offset` (no zig-zag; negatives
# -> 5 bytes). Reuses the certified PacketBuffer (FriendlyByteBuf port).
# Ground truth: tools/PktChatAckSbParity.java.
add_executable(pkt_chat_ack_sb_parity
    network/protocol/game/PktChatAckSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_chat_ack_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_chat_ack_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_chat_ack_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_chat_ack_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.configuration.ClientboundCodeOfConductPacket
# record(String codeOfConduct); STREAM_CODEC = composite(ByteBufCodecs.STRING_UTF8, ...).
# Codec body is exactly one Utf8String.write(out, value, 32767) == writeUtf(s);
# reuses the certified PacketBuffer (Utf8String port: VarInt byte-len + UTF-8 bytes).
# Ground truth: tools/PktCodeOfConductCfgParity.java.
add_executable(pkt_code_of_conduct_cfg_parity
    network/protocol/configuration/PktCodeOfConductCfgParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_code_of_conduct_cfg_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_code_of_conduct_cfg_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_code_of_conduct_cfg_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_code_of_conduct_cfg_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundContainerButtonClickPacket
# Byte-exact parity for the ServerboundContainerButtonClickPacket STREAM_CODEC vs the
# REAL net.minecraft codec. The packet is a record(int containerId, int buttonId) whose
# STREAM_CODEC is StreamCodec.composite(ByteBufCodecs.CONTAINER_ID, ByteBufCodecs.VAR_INT,
# new) (ServerboundContainerButtonClickPacket.java:10-16). CONTAINER_ID resolves to
# FriendlyByteBuf.writeContainerId == VarInt.write and VAR_INT to VarInt.write, and
# StreamCodec.composite adds no id prefix, so the wire body is simply
#   writeVarInt(containerId) + writeVarInt(buttonId)   (plain LEB128, no zig-zag).
# Reuses the certified PacketBuffer (FriendlyByteBuf port) directly; links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz (+ bcrypt on WIN32).
# Ground truth: tools/PktContainerButtonClickSbParity.java.
add_executable(pkt_container_button_click_sb_parity
    network/protocol/game/PktContainerButtonClickSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_button_click_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_button_click_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_button_click_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_button_click_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundContainerSetDataPacket
# Byte-exact parity for ClientboundContainerSetDataPacket vs the REAL STREAM_CODEC.
# Body == FriendlyByteBuf.writeContainerId(containerId) [VarInt] + writeShort(id) [2B BE]
# + writeShort(value) [2B BE] (real 26.1.2 source lines 28-32; writeContainerId ->
# VarInt.write per FriendlyByteBuf line 679; Packet.codec -> StreamCodec.ofMember, no id
# prefix). Reuses the certified PacketBuffer (FriendlyByteBuf port) directly; links its
# full deps (PacketBuffer.cpp + nbt). Ground truth: tools/PktContainerSetDataParity.java.
add_executable(pkt_container_set_data_parity
    network/protocol/game/PktContainerSetDataParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_set_data_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_set_data_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_set_data_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_set_data_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundContainerSlotStateChangedPacket STREAM_CODEC parity.
# Record(int slotId, int containerId, boolean newState). Body (Packet.codec -> no packet-id prefix):
#   write : writeVarInt(slotId); writeContainerId(containerId); writeBoolean(newState)
# FriendlyByteBuf.writeContainerId/readContainerId are plain VarInt.write/read (no zig-zag), so the
# wire is VarInt(slotId) ++ VarInt(containerId) ++ byte(newState?1:0). Reuses the certified
# PacketBuffer (FriendlyByteBuf port): writeVarInt/readVarInt + writeBool/readBool.
# Ground truth: tools/PktContainerSlotStateSbParity.java.
add_executable(pkt_container_slot_state_sb_parity
    network/protocol/game/PktContainerSlotStateSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_slot_state_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_slot_state_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_slot_state_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_slot_state_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_custom_chat_completions_parity
    network/protocol/game/PktCustomChatCompletionsParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_custom_chat_completions_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_custom_chat_completions_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_custom_chat_completions_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_custom_chat_completions_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundDebugSamplePacket
# Byte-exact parity vs the REAL ClientboundDebugSamplePacket.STREAM_CODEC. Body =
# FriendlyByteBuf.writeLongArray(sample) [VarInt(count) + each long BE8, NOT VarLong]
# then writeEnum(debugSampleType) [VarInt(ordinal)]. RemoteDebugSampleType has one
# constant TICK_TIME (ordinal 0). Reuses the certified PacketBuffer (FriendlyByteBuf
# port): writeVarInt LEB128 + writeLong BE8. Links its full closure (NbtIo/Tag for
# writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktDebugSampleParity.java.
add_executable(pkt_debug_sample_parity
    network/protocol/game/PktDebugSampleParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_debug_sample_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_debug_sample_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_debug_sample_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_debug_sample_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_jigsaw_generate_sb_parity
    network/protocol/game/PktJigsawGenerateSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_jigsaw_generate_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_jigsaw_generate_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_jigsaw_generate_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_jigsaw_generate_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundMountScreenOpenPacket
# Byte-exact parity for ClientboundMountScreenOpenPacket vs the REAL STREAM_CODEC.
# Body == FriendlyByteBuf.writeContainerId(containerId)  [== VarInt.write, FBB:679-685]
#       + writeVarInt(inventoryColumns)
#       + writeInt(entityId)   [fixed 4-byte big-endian int, NOT VarInt]
# (real 26.1.2 source lines 28-32; Packet.codec -> StreamCodec.ofMember, no id prefix).
# Reuses the certified PacketBuffer (FriendlyByteBuf port) directly; links its full
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz (+ bcrypt on WIN32).
# Ground truth: tools/PktMountScreenOpenParity.java.
add_executable(pkt_mount_screen_open_parity
    network/protocol/game/PktMountScreenOpenParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_mount_screen_open_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_mount_screen_open_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_mount_screen_open_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_mount_screen_open_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundMoveVehiclePacket
# Byte-exact parity for ClientboundMoveVehiclePacket vs the REAL STREAM_CODEC
# (ClientboundMoveVehiclePacket.java:11-20): StreamCodec.composite over
#   Vec3.STREAM_CODEC   -> position  (writeDouble x; writeDouble y; writeDouble z)
#   ByteBufCodecs.FLOAT -> yRot      (writeFloat)
#   ByteBufCodecs.FLOAT -> xRot      (writeFloat)
# Body is exactly double x,y,z (BE 8B each) then float yRot,xRot (BE 4B each) = 28 bytes,
# no onGround (unlike the serverbound variant). Verifies the certified PacketBuffer
# (FriendlyByteBuf port) directly and round-trips the body back to (x,y,z,yRot,xRot).
# Ground truth: tools/PktMoveVehicleCbParity.java. Links the PacketBuffer closure
# (NbtIo/Tag pulled in for writeNbt symbols) + miniz, bcrypt on WIN32.
add_executable(pkt_move_vehicle_cb_parity
    network/protocol/game/PktMoveVehicleCbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_move_vehicle_cb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_move_vehicle_cb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_move_vehicle_cb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_move_vehicle_cb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_pick_item_block_sb_parity
    network/protocol/game/PktPickItemBlockSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_pick_item_block_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_pick_item_block_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_pick_item_block_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_pick_item_block_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundPickItemFromEntityPacket STREAM_CODEC parity.
# Body = VarInt(id) ++ bool(includeData) -- composite STREAM_CODEC(ByteBufCodecs.VAR_INT,
# ByteBufCodecs.BOOL), no packet-id prefix. id is a signed VarInt (negatives -> 5-byte LEB128),
# includeData is a single 0x00/0x01 byte. Reuses the certified PacketBuffer (FriendlyByteBuf
# port). Ground truth: tools/PktPickItemEntitySbParity.java.
add_executable(pkt_pick_item_entity_sb_parity
    network/protocol/game/PktPickItemEntitySbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_pick_item_entity_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_pick_item_entity_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_pick_item_entity_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_pick_item_entity_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundRenameItemPacket
# Single String name; codec is writeUtf(name)/readUtf(); reuses the certified
# PacketBuffer (Utf8String port). Ground truth: tools/PktRenameItemSbParity.java.
add_executable(pkt_rename_item_sb_parity
    network/protocol/game/PktRenameItemSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_rename_item_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_rename_item_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_rename_item_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_rename_item_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.configuration.ClientboundResetChatPacket byte-exact codec
# parity vs tools/PktResetChatCfgParity.java. The packet's STREAM_CODEC is
# StreamCodec.unit(INSTANCE): encode writes NOTHING (zero-byte body), decode returns the
# singleton consuming no bytes. The C++ side mirrors that with an empty PacketBuffer and
# asserts size()==0 / hex=="" and that the empty Java bytes decode to an empty\exhausted
# buffer. Reuses the certified PacketBuffer (FriendlyByteBuf port); links its full closure
# (NbtIo/Tag pulled in for writeNbt symbols) + miniz.
add_executable(pkt_reset_chat_cfg_parity
    network/protocol/configuration/PktResetChatCfgParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_reset_chat_cfg_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_reset_chat_cfg_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_reset_chat_cfg_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_reset_chat_cfg_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSelectTradePacket STREAM_CODEC parity.
# Body = VarInt(item) (selected merchant recipe index), no packet-id prefix
# (Packet.codec body codec -> StreamCodec.ofMember). The single field is a plain
# VarInt -> exercises the certified PacketBuffer (FriendlyByteBuf port)
# writeVarInt/readVarInt. Ground truth: tools/PktSelectTradeSbParity.java.
add_executable(pkt_select_trade_sb_parity
    network/protocol/game/PktSelectTradeSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_select_trade_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_select_trade_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_select_trade_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_select_trade_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSetCommandBlockPacket
# Byte-exact codec parity vs the REAL STREAM_CODEC. Codec order (write):
#   writeBlockPos(pos) -> writeLong(pos.asLong())  (BE 8-byte long)
#   writeUtf(command)  -> VarInt(byteLen) + UTF-8 bytes
#   writeEnum(mode)    -> VarInt(mode.ordinal())   SEQUENCE=0 AUTO=1 REDSTONE=2
#   writeByte(flags)   -> low 8 bits (bit0 trackOutput, bit1 conditional, bit2 automatic)
# Fully primitive-decomposable; reuses the certified PacketBuffer (FriendlyByteBuf port);
# links its closure (NbtIo/Tag for writeNbt symbols) + miniz, + bcrypt on WIN32.
# Ground truth: tools/PktSetCommandBlockSbParity.java.
add_executable(pkt_set_command_block_sb_parity
    network/protocol/game/PktSetCommandBlockSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_command_block_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_command_block_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_command_block_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_command_block_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ServerboundSetCommandMinecartPacket
# Ground truth: tools/PktSetCommandMinecartSbParity.java.
add_executable(pkt_set_command_minecart_sb_parity
    network/protocol/game/PktSetCommandMinecartSbParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_command_minecart_sb_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_command_minecart_sb_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_command_minecart_sb_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_command_minecart_sb_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetEntityLinkPacket
# Codec: writeInt(sourceId) + writeInt(destId), both big-endian 4-byte ints; no
# packet-id prefix (Packet.codec -> StreamCodec.ofMember). Ground truth:
# tools/PktSetEntityLinkParity.java.
add_executable(pkt_set_entity_link_parity
    network/protocol/game/PktSetEntityLinkParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_entity_link_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_entity_link_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_entity_link_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_entity_link_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetPassengersPacket
# Byte-exact parity vs the REAL ClientboundSetPassengersPacket.STREAM_CODEC. Codec body is
# writeVarInt(vehicle); writeVarIntArray(passengers) [= writeVarInt(count) + each passenger
# id VarInt]. Read side: vehicle=readVarInt(); passengers=readVarIntArray(). Wire layout:
# VarInt vehicle | VarInt count | count * VarInt passengerId. Reuses the certified
# PacketBuffer (FriendlyByteBuf port): writeVarInt/readVarInt are LEB128.
# Links its full closure (NbtIo/Tag for writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktSetPassengersParity.java.
add_executable(pkt_set_passengers_parity
    network/protocol/game/PktSetPassengersParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_passengers_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_passengers_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_passengers_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_passengers_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== Parallel-authored bounded gates (workflow wave 11) =====

# net.minecraft.util.CsvOutput
# net.minecraft.util.CsvOutput
add_executable(csv_output_parity util/CsvOutputParityTest.cpp)
target_include_directories(csv_output_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(csv_output_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.food.FoodConstants
# FoodConstants parity vs net.minecraft.world.food.FoodConstants â€” the hunger/saturation/
# exhaustion magic numbers (data) + the pure helper saturationByModifier(int,float).
# Bit-exact (int decimal, float raw IEEE-754 bits). Ground truth reads the values
# reflectively out of the real class: tools/FoodConstantsParity.java.
add_executable(food_constants_parity
    world/food/FoodConstantsParityTest.cpp
)
target_include_directories(food_constants_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(food_constants_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.StringUtil
# net.minecraft.util.StringUtil (extra helpers: stripColor / filterText / isValidPlayerName,
# plus re-verify truncateStringIfNecessary edge cases + lineCount)
add_executable(hash_common_parity world/level/levelgen/StringUtilExtraParityTest.cpp)
target_include_directories(hash_common_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(hash_common_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.ConstantFloat
# net.minecraft.util.valueproviders.ConstantFloat
add_executable(int_provider_constant_float_parity util/valueproviders/ConstantFloatParityTest.cpp world/level/levelgen/RandomSource.cpp)
target_include_directories(int_provider_constant_float_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(int_provider_constant_float_parity PRIVATE bcrypt)
endif()
set_target_properties(int_provider_constant_float_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.util.valueproviders.ConstantInt
# net.minecraft.util.valueproviders.ConstantInt
# VERIFY-EXISTING: the certified port mc::valueproviders::ConstantInt lives in
# world/level/levelgen/IntProvider.h (header-only); the test parses the GT TSV and
# compares minInclusive/maxInclusive + 8 sample() draws per case bit-for-bit. sample()
# is pure (ignores the rng); the test seeds a LegacyRandomSource (RandomSource.cpp) and
# perturbs it the same way the Java GT did to prove rng-independence.
# Link closure: only RandomSource.cpp (certified LegacyRandomSource) + bcrypt on WIN32;
# IntProvider.h pulls in nothing else (no glm/AABB/Vec3).
add_executable(int_provider_constant_parity
    util/valueproviders/ConstantIntParityTest.cpp
    world/level/levelgen/RandomSource.cpp
)
target_include_directories(int_provider_constant_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
if(WIN32)
    target_link_libraries(int_provider_constant_parity PRIVATE bcrypt)
endif()
set_target_properties(int_provider_constant_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.util.LightCoordsUtil
add_executable(light_coords_util_parity util/LightCoordsUtilParityTest.cpp)
target_include_directories(light_coords_util_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(light_coords_util_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.resources.ResourceLocation
add_executable(rl_chars_parity resources/ResourceLocationCharsParityTest.cpp)
target_include_directories(rl_chars_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(rl_chars_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.world.level.block.SoundType
# net.minecraft.world.level.block.SoundType
# volume()/pitch() float pair for every public-static SoundType constant.
# VERIFIES the ported table in world/level/block/SoundType.h against ground truth
# from tools/SoundTypeParity.java (the REAL net.minecraft SoundType + getVolume/
# getPitch, reflected over the public static finals). Header-only, pure std C++
# (std::bit_cast); no glm/json/miniz/bcrypt and no extra link sources.
add_executable(sound_type_parity world/level/block/SoundTypeParityTest.cpp)
target_include_directories(sound_type_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(sound_type_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== Netcode registry-id layer (foundational): network VarInt-id registries =====
# net.minecraft network registry-id resolution (MOB_EFFECT/SOUND_EVENT/MENU/ENTITY_TYPE/
# PARTICLE_TYPE/DATA_COMPONENT_TYPE/ATTRIBUTE in vanilla getId order). Header-only loader
# (network/NetworkRegistries.h) over the committed asset assets/network_registries.tsv.
add_executable(network_registries_parity network/NetworkRegistriesParityTest.cpp)
target_include_directories(network_registries_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(network_registries_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== Registry-id-backed packet gates (NetworkRegistries-resolved holders) =====

# net.minecraft.network.protocol.game.ClientboundAddEntityPacket
# Byte-exact parity vs the REAL ClientboundAddEntityPacket.STREAM_CODEC.
# Wire body (codec order, ClientboundAddEntityPacket.java:109-121):
#   VarInt(id) ++ UUID(2 longs BE) ++ registry(ENTITY_TYPE)=VarInt(getId)[PLAIN, no +1]
#   ++ double x,y,z (BE) ++ Vec3.LP_STREAM_CODEC(movement)=LpVec3.write
#   ++ byte packDegrees(xRot) ++ byte packDegrees(yRot) ++ byte packDegrees(yHeadRot)
#   ++ VarInt(data)
# Registry-held EntityType resolved name->id via NetworkRegistries (assets/network_registries.tsv).
# Exercises PacketBuffer + NetworkRegistries only (no per-packet header). GT: tools/PktAddEntityParity.java.
add_executable(pkt_add_entity_parity
    network/protocol/game/PktAddEntityParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_add_entity_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_add_entity_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_add_entity_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_add_entity_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundRemoveMobEffectPacket
# Byte-exact parity vs the REAL ClientboundRemoveMobEffectPacket.STREAM_CODEC
# (ClientboundRemoveMobEffectPacket.java:14-21). Codec body in order:
#   (1) ByteBufCodecs.VAR_INT entityId   -> VarInt(entityId)  (ByteBufCodecs.java:102-110)
#   (2) MobEffect.STREAM_CODEC effect    -> VarInt(MOB_EFFECT.getId(effect))  PLAIN, no +1
#       MobEffect.STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.MOB_EFFECT)
#       (MobEffect.java:41); holderRegistry -> registry(asHolderIdMap) plain-id form
#       (ByteBufCodecs.java:560-586; Registry.java:145-149).
# The registry-held effect id is resolved name->id via mc::net::NetworkRegistries over the
# committed assets/network_registries.tsv (gated by network_registries_parity), NOT hard-coded.
add_executable(pkt_remove_mob_effect_parity
    network/protocol/game/PktRemoveMobEffectParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_remove_mob_effect_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_remove_mob_effect_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_remove_mob_effect_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_remove_mob_effect_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSoundEntityPacket
# Byte-exact parity vs the REAL ClientboundSoundEntityPacket.STREAM_CODEC. Body
# (ClientboundSoundEntityPacket.java:43-50):
#   SoundEvent.STREAM_CODEC.encode(output, sound)  -> ByteBufCodecs.holder(SOUND_EVENT):
#                                                     VarInt(getId+1) for a REFERENCE sound
#   writeEnum(source)  = VarInt(ordinal)           (FriendlyByteBuf.java:471-473)
#   writeVarInt(id); writeFloat(volume); writeFloat(pitch); writeLong(seed)
# The registry-held sound is resolved ns:path -> wire id via mc::net::NetworkRegistries
# (certified by network_registries_parity), then +1 applied per the holder() codec. Reuses
# the certified PacketBuffer (FriendlyByteBuf port); links its full closure (NbtIo/Tag for
# writeNbt symbols) + miniz, and bcrypt on WIN32.
# Ground truth: tools/PktSoundEntityParity.java.
add_executable(pkt_sound_entity_parity
    network/protocol/game/PktSoundEntityParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_sound_entity_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_sound_entity_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_sound_entity_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_sound_entity_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSoundPacket byte-exact codec parity
# vs tools/PktSoundParity.java. STREAM_CODEC = Packet.codec(write, ctor) (no id prefix).
# write(RegistryFriendlyByteBuf out) (ClientboundSoundPacket.java 56-65):
#   SoundEvent.STREAM_CODEC.encode(out, sound)  -- holder(SOUND_EVENT, DIRECT) form,
#     REFERENCE holder -> VarInt(BuiltInRegistries.SOUND_EVENT.getId(value)+1)
#     (ByteBufCodecs.java 588-614; SoundEvent.java:26);
#   writeEnum(source)=VarInt(ordinal) [MASTER=0..UI=10] (FriendlyByteBuf 471-473);
#   writeInt(x) writeInt(y) writeInt(z) BE int, each=(int)(coord*8.0) (LOCATION_ACCURACY=8.0F);
#   writeFloat(volume) writeFloat(pitch) BE float; writeLong(seed) BE long.
# Registry id resolved via NetworkRegistries (assets/network_registries.tsv, header-only);
# reuses the certified PacketBuffer (FriendlyByteBuf port). Links its closure
# (NbtIo/Tag for writeNbt symbols) + miniz (+ bcrypt on Windows).
add_executable(pkt_sound_parity
    network/protocol/game/PktSoundParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_sound_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_sound_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_sound_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_sound_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundStopSoundPacket â€” "registry-id layer"
# wave variant. FINDING: this packet carries NO registry-held field. Its codec
# (ClientboundStopSoundPacket.java 40-56) writes:
#   both  -> writeByte(3) + writeEnum(source)=writeVarInt(ordinal) + writeIdentifier(name)=writeUtf(toString)
#   src   -> writeByte(1) + writeEnum(source)
#   name  -> writeByte(2) + writeIdentifier(name)
#   none  -> writeByte(0)
# The Identifier is a PLAIN ResourceLocation (UTF-8 string), NOT a Holder<SoundEvent>/
# registry id (contrast ClientboundSoundPacket which uses holder(SOUND_EVENT)->VarInt(id+1)).
# So mc::net::NetworkRegistries is not required to encode it; the test still loads the
# network registry table as an availability sanity check and fails loudly if the GT ever
# emits a non-"-" registry field for this packet. (FriendlyByteBuf.writeEnum 471-473 /
# writeIdentifier 585-588 / writeUtf 572-579; SoundSource ordinals SoundSource.java 4-14.)
# Reuses the certified PacketBuffer (FriendlyByteBuf port) + header-only NetworkRegistries;
# links its closure (NbtIo/Tag for writeNbt symbols) + miniz (+ bcrypt on Windows).
# Ground truth: tools/PktStopSoundRegParity.java.
add_executable(pkt_stop_sound_reg_parity
    network/protocol/game/PktStopSoundRegParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_stop_sound_reg_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_stop_sound_reg_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_stop_sound_reg_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_stop_sound_reg_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundUpdateMobEffectPacket
# Byte-exact parity for the ClientboundUpdateMobEffectPacket STREAM_CODEC vs the REAL
# net.minecraft codec. Body (ClientboundUpdateMobEffectPacket.write, lines 58-64):
#   writeVarInt(entityId);
#   MobEffect.STREAM_CODEC.encode(effect)   // ByteBufCodecs.holderRegistry(MOB_EFFECT)
#                                           //   == plain VarInt(getId), NO +1 (ByteBufCodecs:573-576)
#   writeVarInt(effectAmplifier); writeVarInt(effectDurationTicks); writeByte(flags);
# Packet.codec -> StreamCodec.ofMember: body only, no id/length prefix. The effect
# Holder<MobEffect> wire id is resolved via mc::net::NetworkRegistries from
# assets/network_registries.tsv (minecraft:mob_effect). Ground truth:
# tools/PktUpdateMobEffectParity.java.
add_executable(pkt_upd_mob_effect_parity
    network/protocol/game/PktUpdateMobEffectParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_upd_mob_effect_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_upd_mob_effect_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_upd_mob_effect_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_upd_mob_effect_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== netcode Component layer: plain-text Component NBT wire form =====
# net.minecraft.network.chat.ComponentSerialization (literal -> root StringTag via writeNbt(Tag)).
add_executable(component_nbt_parity
    network/ComponentNbtParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(component_nbt_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(component_nbt_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(component_nbt_parity PRIVATE bcrypt)
endif()
set_target_properties(component_nbt_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== netcode Component-carrying play packet gates =====

# net.minecraft.network.protocol.game.ClientboundPlayerCombatKillPacket
# Byte-exact packet codec parity vs the REAL net.minecraft STREAM_CODEC. The record's
# codec is StreamCodec.composite(ByteBufCodecs.VAR_INT ::playerId,
# ComponentSerialization.TRUSTED_STREAM_CODEC ::message, ::new), so the wire body is
# VarInt(playerId) ++ Component(message). Restricted to PLAIN-TEXT literal Components,
# which collapse to a root NBT StringTag (08 <u16 MUTF8 len> <bytes>) â€” the only
# Component wire form currently ported. Reuses the certified PacketBuffer + NBT writer.
add_executable(pkt_player_combat_kill_parity
    network/protocol/game/PktPlayerCombatKillParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_player_combat_kill_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_player_combat_kill_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_player_combat_kill_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_player_combat_kill_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundServerDataPacket
# Byte-exact parity vs the REAL ClientboundServerDataPacket.STREAM_CODEC (GT:
# tools/PktServerDataParity.java). Composite codec, field order:
#   motd : ComponentSerialization.TRUSTED_CONTEXT_FREE_STREAM_CODEC -> for a PLAIN
#          literal collapses to a root StringTag (08 <u16 MUTF8 len> <bytes>) via
#          FriendlyByteBuf.writeNbt(Tag); reproduced by PacketBuffer.writeNbt(NbtTag).
#   iconBytes : Optional<byte[]> = optional(BYTE_ARRAY) -> writeBoolean(present)
#               [+ VarInt(len) + raw bytes].
# Reuses the certified PacketBuffer (FriendlyByteBuf port); links its NBT closure
# (NbtIo/Tag for the writeNbt(Tag) symbols) + miniz (+ bcrypt on Windows).
add_executable(pkt_server_data_parity
    network/protocol/common/PktServerDataParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_server_data_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_server_data_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_server_data_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_server_data_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_set_actionbar_text_parity
    network/protocol/game/PktSetActionBarTextParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_actionbar_text_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_actionbar_text_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_actionbar_text_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_actionbar_text_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.network.protocol.game.ClientboundSetSubtitleTextPacket
# Byte-exact parity for the single-Component packet vs the real STREAM_CODEC:
#   record ClientboundSetSubtitleTextPacket(Component text);
#   STREAM_CODEC = StreamCodec.composite(ComponentSerialization.TRUSTED_STREAM_CODEC,
#       ClientboundSetSubtitleTextPacket::text, ClientboundSetSubtitleTextPacket::new);
# Single Component field, NO Optional framing. TRUSTED_STREAM_CODEC codec-encodes the
# Component to NBT then FriendlyByteBuf.writeNbt(tag); a PLAIN literal collapses to a root
# StringTag (08 <u16 MUTF8 len> <bytes>) â€” the whole packet body. The C++ side reproduces it
# with PacketBuffer::writeNbt(NbtTag::string_(text)) on the exact UTF-8 bytes carried in the
# TSV textHex column. Reuses the certified PacketBuffer (FriendlyByteBuf port); links its
# closure (NbtIo/Tag pulled in for writeNbt symbols) + miniz, bcrypt on WIN32.
# Ground truth: tools/PktSetSubtitleTextParity.java.
add_executable(pkt_set_subtitle_text_parity
    network/protocol/game/PktSetSubtitleTextParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_subtitle_text_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_subtitle_text_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_subtitle_text_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_subtitle_text_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== netcode: ClientboundSetTitleTextPacket (single Component text, TRUSTED_STREAM_CODEC) =====
# net.minecraft.network.protocol.game.ClientboundSetTitleTextPacket â€” record(Component text);
# STREAM_CODEC = composite(ComponentSerialization.TRUSTED_STREAM_CODEC, ...). Plain literal
# collapses to a root StringTag emitted via PacketBuffer.writeNbt(NbtTag). No Optional framing.
add_executable(pkt_set_title_text_parity
    network/protocol/game/PktSetTitleTextParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_title_text_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_title_text_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_title_text_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_title_text_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== ClientboundSystemChatPacket: Component content + bool overlay =====
# STREAM_CODEC = StreamCodec.composite(ComponentSerialization.TRUSTED_STREAM_CODEC, ::content,
#                                      ByteBufCodecs.BOOL, ::overlay, ::new)
# Wire (declaration order): content as a root NBT tag (plain literal -> StringTag 08 <u16 len>
# <MUTF8 bytes>) via FriendlyByteBuf.writeNbt(Tag), then a single boolean byte for overlay.
# GT: tools/PktSystemChatParity.java. content NOT optional (no present-bool).
add_executable(pkt_system_chat_parity
    network/protocol/game/PktSystemChatParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_system_chat_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_system_chat_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_system_chat_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_system_chat_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== netcode ItemStack layer (step A): ItemStack.OPTIONAL_STREAM_CODEC, no-component domain =====
add_executable(itemstack_stream_parity
    network/ItemStackStreamParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(itemstack_stream_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(itemstack_stream_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(itemstack_stream_parity PRIVATE bcrypt)
endif()
set_target_properties(itemstack_stream_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== netcode ItemStack-carrying inventory packet gates =====

# ===== netcode packet: ClientboundContainerSetContentPacket.STREAM_CODEC (empty + no-component ItemStacks) =====
add_executable(pkt_container_set_content_parity
    network/protocol/game/PktContainerSetContentParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_set_content_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_set_content_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_set_content_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_set_content_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== netcode packet: ClientboundContainerSetSlotPacket.STREAM_CODEC (no-component ItemStack domain) =====
add_executable(pkt_container_set_slot_parity
    network/protocol/game/PktContainerSetSlotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_container_set_slot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_container_set_slot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_container_set_slot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_container_set_slot_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# net.minecraft.network.protocol.game.ServerboundSetCreativeModeSlotPacket
# Byte-exact parity for the packet STREAM_CODEC (ServerboundSetCreativeModeSlotPacket.java:11-17):
#   StreamCodec.composite(ByteBufCodecs.SHORT, slotNum,
#     ItemStack.validatedStreamCodec(ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC), itemStack, ::new)
# Wire = writeShort(slotNum) | ItemStack OPTIONAL_UNTRUSTED form. validatedStreamCodec.encode
# delegates to the inner codec (validation only on decode, no extra bytes); DELIMITED patch
# codec == plain patch codec for an empty patch, so over the EMPTY + NO-COMPONENT domain the
# ItemStack bytes are exactly VarInt(0) (empty) or VarInt(count) VarInt(itemId) VarInt(0) VarInt(0).
# itemId resolved name->id via NetworkRegistries (minecraft:item). Ground truth:
# tools/PktSetCreativeSlotParity.java (encodes the REAL packet through RegistryFriendlyByteBuf).
add_executable(pkt_set_creative_slot_parity
    network/protocol/game/PktSetCreativeSlotParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_creative_slot_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_creative_slot_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_creative_slot_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_creative_slot_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== ClientboundSetCursorItemPacket: single ItemStack contents (OPTIONAL_STREAM_CODEC) =====
# ClientboundSetCursorItemPacket.java:9-12 -> record(ItemStack contents);
#   STREAM_CODEC = StreamCodec.composite(ItemStack.OPTIONAL_STREAM_CODEC, ::contents, ::new)
# Wire (no-component domain): empty -> VarInt(0); else VarInt(count) VarInt(itemId) VarInt(0) VarInt(0).
# itemId resolved name->id via NetworkRegistries (minecraft:item). GT: tools/PktSetCursorItemParity.java.
add_executable(pkt_set_cursor_item_parity
    network/protocol/game/PktSetCursorItemParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_cursor_item_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_cursor_item_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_cursor_item_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_cursor_item_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# net.minecraft.network.protocol.game.ClientboundSetEquipmentPacket
# Byte-exact parity for ClientboundSetEquipmentPacket vs the REAL STREAM_CODEC
# (Packet.codec(::write, ::new); write() is hand-written, NOT a composite codec).
# Body: writeVarInt(entity) then per-slot writeByte(ordinal | (i!=size-1 ? 0x80 : 0))
# ++ ItemStack.OPTIONAL_STREAM_CODEC.encode(stack). The LAST slot byte has the high
# bit (CONTINUE_MASK = -128 = 0x80) CLEAR; every earlier slot has it SET. The slot byte
# is EquipmentSlot.ordinal() (MAINHAND=0..SADDLE=7), NOT EquipmentSlot.id. ItemStack uses
# the certified no-component wire form (empty->VarInt(0); else VarInt(count) VarInt(itemId)
# VarInt(0) VarInt(0)), item id resolved name->id via NetworkRegistries (minecraft:item).
# Verifies the certified PacketBuffer (FriendlyByteBuf port) directly. No new header.
# Ground truth: tools/PktSetEquipmentParity.java.
add_executable(pkt_set_equipment_parity
    network/protocol/game/PktSetEquipmentParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_equipment_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_equipment_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_equipment_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_equipment_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

add_executable(pkt_set_player_inventory_parity
    network/protocol/game/PktSetPlayerInventoryParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(pkt_set_player_inventory_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(pkt_set_player_inventory_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(pkt_set_player_inventory_parity PRIVATE bcrypt)
endif()
set_target_properties(pkt_set_player_inventory_parity PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ===== netcode ItemStack layer B (start): DataComponentPatch int components =====
add_executable(dcp_int_parity
    network/DataComponentPatchParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(dcp_int_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dcp_int_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(dcp_int_parity PRIVATE bcrypt)
endif()
set_target_properties(dcp_int_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== netcode ItemStack layer B: DataComponentPatch Component/enum/Unit components =====
add_executable(dcp_misc_parity
    network/DataComponentPatchMiscParityTest.cpp
    network/PacketBuffer.cpp
    nbt/NbtIo.cpp
    nbt/Tag.cpp
)
target_include_directories(dcp_misc_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(dcp_misc_parity PRIVATE miniz)
if(WIN32)
    target_link_libraries(dcp_misc_parity PRIVATE bcrypt)
endif()
set_target_properties(dcp_misc_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: UVPair packed-UV (BakedQuad vertex UV packing) =====
add_executable(uvpair_parity render/model/UVPairParityTest.cpp)
target_include_directories(uvpair_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(uvpair_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: FaceBakery.bakeQuad full single-face assembly =====
add_executable(face_bakery_bakequad_parity render/model/FaceBakeryBakeQuadParityTest.cpp)
target_include_directories(face_bakery_bakequad_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(face_bakery_bakequad_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: element-bake helpers (defaultFaceUV + Direction.rotate cull-remap) =====
add_executable(model_element_helpers_parity render/model/ModelElementHelpersParityTest.cpp)
target_include_directories(model_element_helpers_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(model_element_helpers_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: element-level bake (UnbakedCuboidGeometry.bake -> QuadCollection buckets) =====
add_executable(model_element_bake_parity render/model/ModelElementBakeParityTest.cpp)
target_include_directories(model_element_bake_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(model_element_bake_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: model texture-slot resolution (TextureSlots.Resolver '#ref' fixpoint) =====
add_executable(texture_slots_parity render/model/TextureSlotsResolveParityTest.cpp)
target_include_directories(texture_slots_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(texture_slots_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")

# ===== rendering: ResolvedModel parent-chain resolution (findTop ao/guiLight/geometry/slots) =====
add_executable(resolved_model_parity render/model/ResolvedModelResolveParityTest.cpp)
target_include_directories(resolved_model_parity PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(resolved_model_parity PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")