#pragma once

// 1:1 port of the PURE world-height-limit check nested in the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement
//     private static boolean isStartTooCloseToWorldHeightLimits(
//         LevelHeightAccessor heightAccessor,
//         DimensionPadding   dimensionPadding,
//         BoundingBox        centerPieceBb)                       [JigsawPlacement.java:162-172]
//
// This helper is fully self-contained integer math. From its three arguments it
// reads ONLY:
//   * heightAccessor.getMinY() / getMaxY()    -> two ints (the world's build span)
//   * dimensionPadding.bottom() / top()       -> two ints (a pure record)
//   * centerPieceBb.minY() / maxY()           -> two ints
// plus the *identity* short-circuit `dimensionPadding == DimensionPadding.ZERO`.
// There are NO world writes, NO RandomSource, NO registry/datapack, NO BlockState.
// (The surrounding generateJigsaw(...) that *produces* the centerPieceBb walks the
// pool with a RandomSource and is NOT ported here; only this pure gate is.)
//
// Java source (JigsawPlacement.java:162-172):
//     if (dimensionPadding == DimensionPadding.ZERO) {
//        return false;
//     }
//     int minYWithPadding = heightAccessor.getMinY() + dimensionPadding.bottom();
//     int maxYWithPadding = heightAccessor.getMaxY() - dimensionPadding.top();
//     return centerPieceBb.minY() < minYWithPadding || centerPieceBb.maxY() > maxYWithPadding;
//
// 1:1 TRAPS faithfully replicated below:
//   * The short-circuit is REFERENCE identity (`== DimensionPadding.ZERO`), NOT a
//     value comparison. A padding parsed from data with bottom==0 && top==0 is a
//     fresh record instance and is NOT `== ZERO`, so it does NOT short-circuit —
//     it falls through to the arithmetic (which, for (0,0), happens to give the
//     same answer, but a value-equality port would also wrongly short-circuit a
//     hypothetical asymmetric-but-both-zero case... more importantly it documents
//     the real control flow). We carry an explicit `isZeroSentinel` flag from the
//     ground truth so the port reproduces the exact branch the real code took.
//   * LevelHeightAccessor.getMaxY() == getMinY() + getHeight() - 1
//     (LevelHeightAccessor.java:11-13). The ground truth feeds us getMinY()/
//     getMaxY() directly; this header only does the additions/subtractions.
//   * `minY + bottom` / `maxY - top` are Java int arithmetic (two's-complement
//     wrap). C++ signed overflow is UB, so we route the add/sub through uint32_t.
//   * The comparison is strict `<` on the low side and strict `>` on the high
//     side (a box exactly flush with the padded bounds is OK, not too-close).
//
// Certified byte-exact by jigsaw_height_limit_parity
// (tools/JigsawHeightLimitParity.java drives the REAL JigsawPlacement helper via
// reflection over a concrete LevelHeightAccessor + DimensionPadding battery and
// emits a TSV; this header recomputes and compares).

#include <cstdint>

namespace mc::levelgen::structure::pools {

// Java int arithmetic wraps on overflow (two's complement). C++ signed overflow is
// UB, so route every add/sub through uint32_t and reinterpret, keeping the port
// byte-identical to Java for every input.
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}

// net.minecraft.world.level.levelgen.structure.pools.DimensionPadding (record).
// Only bottom()/top() are read by this helper. `isZeroSentinel` mirrors Java
// reference identity with the static DimensionPadding.ZERO constant — it is true
// ONLY when the value passed is literally that singleton, never merely (0,0).
struct DimensionPadding {
    int32_t bottom{};
    int32_t top{};
    bool isZeroSentinel{false};
};

// net.minecraft.world.level.levelgen.structure.BoundingBox — only minY()/maxY()
// are read here.
struct HeightSpanBox {
    int32_t minY{};
    int32_t maxY{};
};

// JigsawPlacement.isStartTooCloseToWorldHeightLimits(...) — JigsawPlacement.java
// :162-172. `worldMinY`/`worldMaxY` are heightAccessor.getMinY()/getMaxY().
constexpr bool isStartTooCloseToWorldHeightLimits(int32_t worldMinY, int32_t worldMaxY,
                                                  const DimensionPadding& dimensionPadding,
                                                  const HeightSpanBox& centerPieceBb) noexcept {
    // dimensionPadding == DimensionPadding.ZERO  (REFERENCE identity, not value).
    if (dimensionPadding.isZeroSentinel) {
        return false;
    }
    int32_t minYWithPadding = iadd(worldMinY, dimensionPadding.bottom);
    int32_t maxYWithPadding = isub(worldMaxY, dimensionPadding.top);
    return centerPieceBb.minY < minYWithPadding || centerPieceBb.maxY > maxYWithPadding;
}

} // namespace mc::levelgen::structure::pools
