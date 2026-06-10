// 1:1 port of the net.minecraft.world.level.levelgen.Heightmap.Types enum
// (Minecraft 26.1.2) — the ENUM PART ONLY (no heightmap data array / BitStorage).
//
// Source: 26.1.2/src/net/minecraft/world/level/levelgen/Heightmap.java
//         (enum Types, lines 146-194; enum Usage, lines 196-200)
//         26.1.2/src/net/minecraft/util/ByIdMap.java (continuous / ZERO).
//
// Each Types constant carries (verbatim from the Java ctor calls):
//   WORLD_SURFACE_WG          (0, "WORLD_SURFACE_WG",          Usage.WORLDGEN,   NOT_AIR)
//   WORLD_SURFACE             (1, "WORLD_SURFACE",             Usage.CLIENT,     NOT_AIR)
//   OCEAN_FLOOR_WG            (2, "OCEAN_FLOOR_WG",            Usage.WORLDGEN,   MATERIAL_MOTION_BLOCKING)
//   OCEAN_FLOOR               (3, "OCEAN_FLOOR",               Usage.LIVE_WORLD, MATERIAL_MOTION_BLOCKING)
//   MOTION_BLOCKING           (4, "MOTION_BLOCKING",           Usage.CLIENT,     <blocksMotion || !fluid.isEmpty>)
//   MOTION_BLOCKING_NO_LEAVES (5, "MOTION_BLOCKING_NO_LEAVES", Usage.CLIENT,     <(blocksMotion || !fluid.isEmpty) && !LeavesBlock>)
//
// id == ordinal (declaration index). serializationKey == name == getSerializedName().
//
// Accessor semantics (verbatim from Heightmap.Types):
//   getSerializationKey() / getSerializedName() == this.serializationKey
//   sendToClient()        == this.usage == Usage.CLIENT
//   keepAfterWorldgen()   == this.usage != Usage.WORLDGEN
//   isOpaque()            == the BlockState predicate (one of four predicate
//                            *categories* below). The predicate BODIES require a
//                            live BlockState (isAir/blocksMotion/getFluidState/
//                            getBlock instanceof LeavesBlock) so are NOT evaluated
//                            here — we model only WHICH predicate each Types uses,
//                            which is the byte-identity this gate certifies.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace mc::levelgen::heightmap {

// net.minecraft.world.level.levelgen.Heightmap.Usage — declaration order is the
// ordinal (not otherwise used here, but kept exact: WORLDGEN=0, LIVE_WORLD=1, CLIENT=2).
enum class Usage : std::int32_t {
    WORLDGEN = 0,
    LIVE_WORLD = 1,
    CLIENT = 2,
};

// The four distinct BlockState predicates referenced by the Types ctors. We model
// their IDENTITY only (the predicate body needs a live BlockState). Two constants
// share NOT_AIR and two share MATERIAL_MOTION_BLOCKING, exactly as in the Java —
// matching the `Heightmap.NOT_AIR` / `Heightmap.MATERIAL_MOTION_BLOCKING` static
// fields and the two inline lambdas.
enum class OpaquePredicate : std::int32_t {
    NOT_AIR = 0,                            // !state.isAir()
    MATERIAL_MOTION_BLOCKING = 1,           // state.blocksMotion()
    MOTION_BLOCKING = 2,                    // blocksMotion() || !getFluidState().isEmpty()
    MOTION_BLOCKING_NO_LEAVES = 3,          // (above) && !(getBlock() instanceof LeavesBlock)
};

// Heightmap.Types constants, declaration order == ordinal.
enum class Types : std::int32_t {
    WORLD_SURFACE_WG = 0,
    WORLD_SURFACE = 1,
    OCEAN_FLOOR_WG = 2,
    OCEAN_FLOOR = 3,
    MOTION_BLOCKING = 4,
    MOTION_BLOCKING_NO_LEAVES = 5,
};

inline constexpr std::int32_t TYPES_COUNT = 6;

struct TypesData {
    std::int32_t id;                  // == ordinal; the `id` ctor arg
    std::string_view serializationKey;  // getSerializationKey() / getSerializedName()
    Usage usage;                      // this.usage
    OpaquePredicate isOpaque;         // identity of this.isOpaque
};

// Verbatim from the six Types ctor calls (declaration order == ordinal == id).
inline constexpr std::array<TypesData, 6> TYPES{{
    TypesData{0, "WORLD_SURFACE_WG", Usage::WORLDGEN, OpaquePredicate::NOT_AIR},
    TypesData{1, "WORLD_SURFACE", Usage::CLIENT, OpaquePredicate::NOT_AIR},
    TypesData{2, "OCEAN_FLOOR_WG", Usage::WORLDGEN, OpaquePredicate::MATERIAL_MOTION_BLOCKING},
    TypesData{3, "OCEAN_FLOOR", Usage::LIVE_WORLD, OpaquePredicate::MATERIAL_MOTION_BLOCKING},
    TypesData{4, "MOTION_BLOCKING", Usage::CLIENT, OpaquePredicate::MOTION_BLOCKING},
    TypesData{5, "MOTION_BLOCKING_NO_LEAVES", Usage::CLIENT, OpaquePredicate::MOTION_BLOCKING_NO_LEAVES},
}};

inline constexpr const TypesData& data(Types t) {
    return TYPES[static_cast<std::size_t>(t)];
}

// public String getSerializationKey() { return this.serializationKey; }
inline constexpr std::string_view getSerializationKey(const TypesData& t) {
    return t.serializationKey;
}
// @Override public String getSerializedName() { return this.serializationKey; }
inline constexpr std::string_view getSerializedName(const TypesData& t) {
    return t.serializationKey;
}
// public boolean sendToClient() { return this.usage == Heightmap.Usage.CLIENT; }
inline constexpr bool sendToClient(const TypesData& t) {
    return t.usage == Usage::CLIENT;
}
// public boolean keepAfterWorldgen() { return this.usage != Heightmap.Usage.WORLDGEN; }
inline constexpr bool keepAfterWorldgen(const TypesData& t) {
    return t.usage != Usage::WORLDGEN;
}
// Identity of public Predicate<BlockState> isOpaque() { return this.isOpaque; }
inline constexpr OpaquePredicate isOpaque(const TypesData& t) { return t.isOpaque; }

// Heightmap.Types.BY_ID == ByIdMap.continuous(t -> t.id, values(), ZERO):
//   id >= 0 && id < length ? sortedValues[id] : sortedValues[0]
// (ids are continuous 0..5, so sortedValues == declaration order.)
inline constexpr const TypesData& byId(std::int32_t id) {
    if (id >= 0 && id < TYPES_COUNT) {
        return TYPES[static_cast<std::size_t>(id)];
    }
    return TYPES[0];  // WORLD_SURFACE_WG
}

}  // namespace mc::levelgen::heightmap
