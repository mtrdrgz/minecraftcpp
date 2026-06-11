#pragma once

// 1:1 port of the PURE template-placement-position math nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.IglooPieces
//     -> public static class IglooPiece
//          private static BlockPos makePosition(Identifier templateLocation,
//                                               BlockPos position, int depth)  [line 101-103]
//
// makePosition is fully self-contained integer geometry:
//   return position.offset(OFFSETS.get(templateLocation)).below(depth);
// It does NO world writes, generates NO RandomSource values, and touches NO
// registry/datapack — OFFSETS is a hard-coded ImmutableMap of compile-time
// BlockPos constants nested in IglooPieces (IglooPieces.java:41-43). The three
// template keys are the only valid inputs; their offsets are baked in below.
//
// The two BlockPos ops it composes (net.minecraft.core.BlockPos):
//   * offset(Vec3i)  == offset(x,y,z) == new BlockPos(getX()+x, getY()+y, getZ()+z)
//                       (BlockPos.java:122-136),
//   * below(int steps) == relative(Direction.DOWN, steps)
//                       == new BlockPos(x + 0*steps, y + (-1)*steps, z + 0*steps)
//                       (BlockPos.java:162-164, 202-206; Direction.DOWN.getStepY()
//                        == -1). i.e. y -= steps, x/z unchanged.
// `depth` at the call sites is depth*3 / i*3 where depth = random.nextInt(8)+4
// (IglooPieces.java:52-57), so it spans 0..(11*3)=33; we probe a wider band
// including large + negative values to pin the int subtraction.
//
// Certified byte-exact by igloo_piece_position_parity
// (tools/IglooPiecePositionParity.java drives the REAL IglooPiece.makePosition
// via reflection over the REAL OFFSETS map and emits a TSV; this header
// recomputes and compares — pure-int, exact decimal compare).

#include <cstdint>
#include <string>

namespace mc::levelgen::structure::igloo {

// Java int arithmetic wraps on two's-complement overflow. C++ signed overflow is
// UB, so route the adds/muls through uint32_t so the port is byte-identical to
// Java for every input (the GT battery stays in a benign range, but this makes the
// port correct by construction at -O2).
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t imul(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}

// ── net.minecraft.core.Vec3i / BlockPos (the immutable int triple) ───────────
struct BlockPos {
    int32_t x{}, y{}, z{};

    constexpr BlockPos() = default;
    constexpr BlockPos(int32_t x_, int32_t y_, int32_t z_) noexcept : x(x_), y(y_), z(z_) {}

    constexpr bool operator==(const BlockPos&) const = default;

    // BlockPos.offset(int,int,int) — BlockPos.java:122-124. (The x==0&&y==0&&z==0
    // "return this" fast-path is a pure object-identity optimization; the produced
    // coordinates are identical, so a plain componentwise add is 1:1.)
    constexpr BlockPos offset(int32_t dx, int32_t dy, int32_t dz) const noexcept {
        return BlockPos(iadd(x, dx), iadd(y, dy), iadd(z, dz));
    }
    // BlockPos.offset(Vec3i) — BlockPos.java:134-136.
    constexpr BlockPos offset(const BlockPos& v) const noexcept {
        return offset(v.x, v.y, v.z);
    }

    // BlockPos.below(int steps) == relative(Direction.DOWN, steps) — Direction.DOWN
    // has stepX=0, stepY=-1, stepZ=0, so result = (x, y - steps, z). The steps==0
    // "return this" fast-path again only affects identity, not coordinates.
    // BlockPos.java:162-164 + relative(Direction,int) at :202-206.
    constexpr BlockPos below(int32_t steps) const noexcept {
        return BlockPos(iadd(x, imul(0, steps)),
                        iadd(y, imul(-1, steps)),
                        iadd(z, imul(0, steps)));
    }
};

// IglooPieces.OFFSETS — the hard-coded ImmutableMap<Identifier,BlockPos>
// (IglooPieces.java:41-43). Keys are the three igloo template Identifiers; we key
// on their string form (namespace defaults to "minecraft"):
//   "minecraft:igloo/top"    -> BlockPos.ZERO  (0,0,0)
//   "minecraft:igloo/middle" -> ( 2,-3, 4)
//   "minecraft:igloo/bottom" -> ( 0,-3,-2)
constexpr BlockPos kOffsetTop{0, 0, 0};
constexpr BlockPos kOffsetMiddle{2, -3, 4};
constexpr BlockPos kOffsetBottom{0, -3, -2};

// Returns the OFFSETS entry for a template key, or {0,0,0} for an unknown key.
// (The real map only ever holds the three keys; unknown keys never occur at the
// call sites. We model that explicitly rather than silently passing.)
inline BlockPos offsetForTemplate(const std::string& templateLocation) noexcept {
    if (templateLocation == "minecraft:igloo/top")    return kOffsetTop;
    if (templateLocation == "minecraft:igloo/middle") return kOffsetMiddle;
    if (templateLocation == "minecraft:igloo/bottom") return kOffsetBottom;
    return BlockPos{0, 0, 0};
}

// IglooPieces.IglooPiece.makePosition(Identifier, BlockPos, int) —
// IglooPieces.java:101-103:
//   return position.offset(OFFSETS.get(templateLocation)).below(depth);
inline BlockPos makePosition(const std::string& templateLocation,
                             const BlockPos& position, int32_t depth) noexcept {
    return position.offset(offsetForTemplate(templateLocation)).below(depth);
}

} // namespace mc::levelgen::structure::igloo
