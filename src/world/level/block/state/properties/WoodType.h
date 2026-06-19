// 1:1 C++ port of the DATA surface of
//   net.minecraft.world.level.block.state.properties.WoodType
// (Minecraft 26.1.2). New file — does not edit any shared header.
//
// WoodType is a record:
//   (String name, BlockSetType setType, SoundType soundType,
//    SoundType hangingSignSoundType, SoundEvent fenceGateClose,
//    SoundEvent fenceGateOpen)
//
// All values below are transcribed VERBATIM from
//   26.1.2/src/net/minecraft/world/level/block/state/properties/WoodType.java
// and the BlockSetType data it references from
//   26.1.2/src/net/minecraft/world/level/block/state/properties/BlockSetType.java
//
// PORTED (pure data, no registry/world/component coupling required):
//   * the ordered set of WoodTypes (insertion order into the
//     Object2ObjectArrayMap TYPES — which is exactly the static-field
//     declaration order), exposed via values()
//   * each WoodType's name()
//   * each WoodType's setType().name()
//   * the BlockSetType booleans canOpenByHand / canOpenByWindCharge /
//     canButtonBeActivatedByArrows and pressurePlateSensitivity (enum ordinal)
//   * the SoundEvent fence-gate close/open *locations* (the registry id strings
//     passed at registration, namespaced "minecraft:")
//   * a SoundType "category" identity (which named SoundType constant the
//     soundType()/hangingSignSoundType() fields point at — a pure consequence
//     of which constructor was used), encoded as an enum
//   * CODEC behaviour: name -> WoodType resolution (Codec.stringResolver over
//     TYPES::get), i.e. byName(name)
//
// NOT PORTED (registry/asset-coupled object graph; out of scope for a data gate):
//   * the SoundType objects themselves (their volume/pitch and per-block
//     SoundEvent fields) — only their identity category is captured here
//   * the SoundEvent objects' fixedRange / Holder wiring — only the location id
//
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace mc::block::state::properties {

// BlockSetType.PressurePlateSensitivity — declared { EVERYTHING, MOBS; }
//   => EVERYTHING.ordinal()==0, MOBS.ordinal()==1
enum class PressurePlateSensitivity : int32_t {
    EVERYTHING = 0,
    MOBS = 1,
};

// Identity category of the SoundType used by a WoodType.
// WoodType's 2-arg ctor defaults soundType=SoundType.WOOD,
// hangingSignSoundType=SoundType.HANGING_SIGN (=> DEFAULT_WOOD). The 6-arg
// registrations pass {CHERRY_WOOD, CHERRY_WOOD_HANGING_SIGN},
// {NETHER_WOOD, NETHER_WOOD_HANGING_SIGN} or {BAMBOO_WOOD,
// BAMBOO_WOOD_HANGING_SIGN}.
enum class WoodSoundCategory : int32_t {
    DEFAULT_WOOD = 0,  // SoundType.WOOD        + SoundType.HANGING_SIGN
    CHERRY_WOOD = 1,   // SoundType.CHERRY_WOOD + SoundType.CHERRY_WOOD_HANGING_SIGN
    NETHER_WOOD = 2,   // SoundType.NETHER_WOOD + SoundType.NETHER_WOOD_HANGING_SIGN
    BAMBOO_WOOD = 3,   // SoundType.BAMBOO_WOOD + SoundType.BAMBOO_WOOD_HANGING_SIGN
};

// The portable data of a BlockSetType (only the fields a wood type exposes that
// are not registry/asset object references).
struct BlockSetTypeData {
    std::string_view name;
    bool canOpenByHand;
    bool canOpenByWindCharge;
    bool canButtonBeActivatedByArrows;
    PressurePlateSensitivity pressurePlateSensitivity;
};

// BlockSetType "wood-default" constructor (single-arg `BlockSetType(name)`):
//   canOpenByHand=true, canOpenByWindCharge=true,
//   canButtonBeActivatedByArrows=true, sensitivity=EVERYTHING.
constexpr BlockSetTypeData woodDefaultSet(std::string_view name) {
    return BlockSetTypeData{name, true, true, true, PressurePlateSensitivity::EVERYTHING};
}

struct WoodTypeData {
    std::string_view name;
    BlockSetTypeData setType;
    WoodSoundCategory soundCategory;
    std::string_view fenceGateCloseLocation;  // namespaced "minecraft:..."
    std::string_view fenceGateOpenLocation;
};

// === The 12 registered WoodTypes, in TYPES insertion order ===
// (= static field declaration order in WoodType.java, lines 16..63:
//  OAK, SPRUCE, BIRCH, ACACIA, CHERRY, JUNGLE, DARK_OAK, PALE_OAK,
//  CRIMSON, WARPED, MANGROVE, BAMBOO)
inline constexpr std::array<WoodTypeData, 12> WOOD_TYPES = {{
    // OAK
    {"oak", woodDefaultSet("oak"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // SPRUCE
    {"spruce", woodDefaultSet("spruce"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // BIRCH
    {"birch", woodDefaultSet("birch"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // ACACIA
    {"acacia", woodDefaultSet("acacia"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // CHERRY — BlockSetType.CHERRY: canOpenByHand=true, byWindCharge=true,
    //          buttonByArrows=true, sensitivity=EVERYTHING.
    {"cherry",
     BlockSetTypeData{"cherry", true, true, true, PressurePlateSensitivity::EVERYTHING},
     WoodSoundCategory::CHERRY_WOOD,
     "minecraft:block.cherry_wood_fence_gate.close",
     "minecraft:block.cherry_wood_fence_gate.open"},
    // JUNGLE
    {"jungle", woodDefaultSet("jungle"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // DARK_OAK
    {"dark_oak", woodDefaultSet("dark_oak"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // PALE_OAK
    {"pale_oak", woodDefaultSet("pale_oak"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // CRIMSON — BlockSetType.CRIMSON: all-true, EVERYTHING.
    {"crimson",
     BlockSetTypeData{"crimson", true, true, true, PressurePlateSensitivity::EVERYTHING},
     WoodSoundCategory::NETHER_WOOD,
     "minecraft:block.nether_wood_fence_gate.close",
     "minecraft:block.nether_wood_fence_gate.open"},
    // WARPED — BlockSetType.WARPED: all-true, EVERYTHING.
    {"warped",
     BlockSetTypeData{"warped", true, true, true, PressurePlateSensitivity::EVERYTHING},
     WoodSoundCategory::NETHER_WOOD,
     "minecraft:block.nether_wood_fence_gate.close",
     "minecraft:block.nether_wood_fence_gate.open"},
    // MANGROVE
    {"mangrove", woodDefaultSet("mangrove"), WoodSoundCategory::DEFAULT_WOOD,
     "minecraft:block.fence_gate.close", "minecraft:block.fence_gate.open"},
    // BAMBOO — BlockSetType.BAMBOO: all-true, EVERYTHING.
    {"bamboo",
     BlockSetTypeData{"bamboo", true, true, true, PressurePlateSensitivity::EVERYTHING},
     WoodSoundCategory::BAMBOO_WOOD,
     "minecraft:block.bamboo_wood_fence_gate.close",
     "minecraft:block.bamboo_wood_fence_gate.open"},
}};

// values() — the registered wood types in TYPES iteration order.
// (Object2ObjectArrayMap preserves insertion order.)
inline constexpr int WOOD_TYPE_COUNT = 12;

// WoodType.CODEC = Codec.stringResolver(WoodType::name, TYPES::get).
// Resolution by name == TYPES.get(name): present -> the WoodType, else empty.
inline std::optional<WoodTypeData> byName(std::string_view name) {
    for (int i = 0; i < WOOD_TYPE_COUNT; ++i) {
        if (WOOD_TYPES[static_cast<std::size_t>(i)].name == name) {
            return WOOD_TYPES[static_cast<std::size_t>(i)];
        }
    }
    return std::nullopt;
}

}  // namespace mc::block::state::properties
