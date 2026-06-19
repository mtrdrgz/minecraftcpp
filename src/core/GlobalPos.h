#pragma once
// 1:1 C++ port of net.minecraft.core.GlobalPos (Minecraft 26.1.2).
//
// GlobalPos is a record `(ResourceKey<Level> dimension, BlockPos pos)`. This
// header ports the byte-deterministic surface of that record plus the BlockPos /
// Vec3i helpers it relies on. Everything here is verbatim from:
//   26.1.2/src/net/minecraft/core/GlobalPos.java
//   26.1.2/src/net/minecraft/core/BlockPos.java
//   26.1.2/src/net/minecraft/core/Vec3i.java
//   26.1.2/src/net/minecraft/resources/ResourceKey.java
//   26.1.2/src/net/minecraft/resources/Identifier.java
//
// Dimension model: GlobalPos always builds its ResourceKey via Registries.DIMENSION,
// so the ResourceKey's registryName is fixed to "minecraft:dimension"
// (Registries.DIMENSION = createRegistryKey("dimension") -> Identifier
// withDefaultNamespace("dimension")). The only varying part is the dimension's own
// Identifier (namespace + path). We therefore store the dimension as that Identifier.
//
// NOT ported (intentionally — see GlobalPosParityTest notes):
//   * GlobalPos.hashCode(): a record hashCode combines component hashCodes; the
//     ResourceKey component has NO equals/hashCode override, so it falls back to
//     Object identity-hashCode (interned instance). That is non-deterministic across
//     JVM runs and cannot be byte-matched. (BlockPos's own hashCode IS deterministic
//     and is ported here.)
//   * MAP_CODEC / CODEC / STREAM_CODEC: serialization, explicitly out of scope.

#include <cstdint>
#include <string>

namespace mc::globalpos {

// --- BlockPos / Vec3i long-packing constants (BlockPos.java lines 49-57) -------
// PACKED_HORIZONTAL_LENGTH = 1 + log2(smallestEncompassingPowerOfTwo(30000000))
//   smallestEncompassingPowerOfTwo(30000000) = 2^25 = 33554432, log2 = 25 -> 26
inline constexpr int PACKED_HORIZONTAL_LENGTH = 26;
// PACKED_Y_LENGTH = 64 - 2 * PACKED_HORIZONTAL_LENGTH = 64 - 52 = 12
inline constexpr int PACKED_Y_LENGTH = 64 - 2 * PACKED_HORIZONTAL_LENGTH;
inline constexpr int64_t PACKED_X_MASK = (int64_t(1) << PACKED_HORIZONTAL_LENGTH) - 1;
inline constexpr int64_t PACKED_Y_MASK = (int64_t(1) << PACKED_Y_LENGTH) - 1;
inline constexpr int64_t PACKED_Z_MASK = (int64_t(1) << PACKED_HORIZONTAL_LENGTH) - 1;
inline constexpr int Y_OFFSET = 0;
inline constexpr int Z_OFFSET = PACKED_Y_LENGTH;             // 12
inline constexpr int X_OFFSET = PACKED_Y_LENGTH + PACKED_HORIZONTAL_LENGTH; // 38

// BlockPos.asLong(x, y, z) — BlockPos.java lines 111-116.
inline int64_t blockPosAsLong(int32_t x, int32_t y, int32_t z) {
    int64_t node = 0;
    node |= (int64_t(x) & PACKED_X_MASK) << X_OFFSET;
    node |= (int64_t(y) & PACKED_Y_MASK) << Y_OFFSET;
    return node | ((int64_t(z) & PACKED_Z_MASK) << Z_OFFSET);
}

// Vec3i.hashCode() — Vec3i.java lines 52-55: (y + z*31)*31 + x, two's-complement
// int wraparound (Java int arithmetic). BlockPos inherits this unchanged.
inline int32_t blockPosHashCode(int32_t x, int32_t y, int32_t z) {
    return (y + z * 31) * 31 + x; // wraps in int32 exactly like Java
}

// Vec3i.distChessboard(pos) — Vec3i.java lines 230-235.
inline int32_t distChessboard(int32_t ax, int32_t ay, int32_t az,
                              int32_t bx, int32_t by, int32_t bz) {
    auto jabs = [](int32_t v) -> int32_t {
        return v < 0 ? -v : v; // Math.abs; abs(INT_MIN)==INT_MIN (irrelevant here)
    };
    int32_t xd = jabs(ax - bx);
    int32_t yd = jabs(ay - by);
    int32_t zd = jabs(az - bz);
    int32_t m = xd > yd ? xd : yd;
    return m > zd ? m : zd;
}

// --- GlobalPos record ----------------------------------------------------------
struct BlockPos {
    int32_t x = 0, y = 0, z = 0;
    bool operator==(const BlockPos&) const = default;

    // Vec3i.equals (lines 43-50): same x,y,z.
    int64_t asLong() const { return blockPosAsLong(x, y, z); }
    int32_t hashCode() const { return blockPosHashCode(x, y, z); }
    // Vec3i.toString via MoreObjects.toStringHelper(this) -> uses runtime simple
    // name "BlockPos": "BlockPos{x=.., y=.., z=..}" (Vec3i.java lines 245-248).
    std::string toString() const {
        return "BlockPos{x=" + std::to_string(x) + ", y=" + std::to_string(y) +
               ", z=" + std::to_string(z) + "}";
    }
};

struct GlobalPos {
    // ResourceKey<Level> dimension, represented by its Identifier (namespace, path).
    // The registry name is fixed to minecraft:dimension (see header comment).
    std::string dimNamespace;
    std::string dimPath;
    BlockPos pos;

    // Record equals: ResourceKey identity == structural identity (interned), so two
    // dimension keys are equal iff same registry (always minecraft:dimension) and
    // same identifier (namespace+path); BlockPos equals is x/y/z.
    bool equals(const GlobalPos& o) const {
        return dimNamespace == o.dimNamespace && dimPath == o.dimPath && pos == o.pos;
    }

    // dimensionEquals: ResourceKey.equals(other) — identity of interned key, i.e.
    // same registry + identifier. (registry fixed; compare identifier.)
    bool dimensionEquals(const std::string& ns, const std::string& path) const {
        return dimNamespace == ns && dimPath == path;
    }

    // GlobalPos.toString (GlobalPos.java lines 26-29): dimension + " " + pos.
    // ResourceKey.toString (ResourceKey.java 42-45):
    //   "ResourceKey[" + registryName + " / " + identifier + "]"
    // registryName = minecraft:dimension; identifier = <ns>:<path>.
    std::string toString() const {
        return "ResourceKey[minecraft:dimension / " + dimNamespace + ":" + dimPath +
               "] " + pos.toString();
    }

    // GlobalPos.isCloseEnough (GlobalPos.java lines 31-33).
    bool isCloseEnough(const std::string& ns, const std::string& path,
                       int32_t px, int32_t py, int32_t pz, int32_t maxDistance) const {
        return dimensionEquals(ns, path) &&
               distChessboard(pos.x, pos.y, pos.z, px, py, pz) <= maxDistance;
    }
};

} // namespace mc::globalpos
