#pragma once
// 1:1 port of the PURE cluster-ruin placement geometry in the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanRuinPieces
//
// Covered, world-free, deterministic methods (no Level / registry / GL):
//
//   (A) allPositions(RandomSource, BlockPos origin)            [OceanRuinPieces.java:199-210]
//       The eight candidate cluster-ruin offsets. Each axis offset is
//         <constant> + Mth.nextInt(random, lo, hi)
//       and the BlockPos is origin.offset(dx, 0, dz). PURE: only RandomSource
//       draws + int adds. The exact draw order (8 positions, each one X-draw
//       then one Z-draw, list-order) is what couples this to vanilla seeding.
//
//   (B) The parent bounding-box + per-candidate fit test that addClusterRuins
//       performs around allPositions [OceanRuinPieces.java:168-197]:
//         parentPos     = (p.x, 90, p.z)
//         parentCorner  = StructureTemplate.transform((15,0,15), Mirror.NONE,
//                                                      rotation, BlockPos.ZERO)
//                           .offset(parentPos)
//         parentBB      = BoundingBox.fromCorners(parentPos, parentCorner)
//         parentBottomLeft = (min(parentPos.x,parentCorner.x), parentPos.y,
//                             min(parentPos.z,parentCorner.z))
//       and, for a placed candidate at `pos` with `nextRotation`:
//         nextCorner    = StructureTemplate.transform((5,0,6), Mirror.NONE,
//                                                      nextRotation, BlockPos.ZERO)
//                           .offset(pos)
//         nextBB        = BoundingBox.fromCorners(pos, nextCorner)
//         fits          = !nextBB.intersects(parentBB)
//       All of these are pure int/box math (StructureTemplate.transform is the
//       already-gated rotation transform; reproduced here inline so this header
//       is self-contained, and re-certified against the real method by the GT).
//
// The Mth.nextInt / Util.getRandom / Rotation.getRandom RNG draws (used by
// addClusterRuins between geometry steps) are NOT reproduced here — they belong
// to the RandomSource gate, not to this geometry gate. This header ports only the
// pure positional arithmetic; the GT tool drives the REAL methods to produce the
// expected values.
//
// Certified byte-exact by ocean_ruin_cluster_parity
// (tools/OceanRuinClusterParity.java).

#include <array>
#include <cstdint>

namespace mc::levelgen::structure::oceanruin {

// ---- Java int wrap helpers (signed overflow is UB at -O2) -------------------
// Every add/sub that can overflow goes through uint32_t then bit-casts back, so
// the result matches Java's two's-complement wrap exactly.
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
constexpr int32_t imin(int32_t a, int32_t b) noexcept { return a < b ? a : b; }

// net.minecraft.core.BlockPos (Vec3i) — only the int fields matter here.
struct BlockPos {
    int32_t x{}, y{}, z{};
    constexpr bool operator==(const BlockPos&) const = default;
    // Vec3i.offset(int,int,int) — Vec3i.java:92-94. The (0,0,0) short-circuit
    // returns `this`; value-wise identical to the additions, so we always add.
    constexpr BlockPos offset(int32_t dx, int32_t dy, int32_t dz) const noexcept {
        return BlockPos{iadd(x, dx), iadd(y, dy), iadd(z, dz)};
    }
};

// net.minecraft.world.level.levelgen.structure.BoundingBox — the swap-on-invert
// ctor and fromCorners + intersects, ported 1:1 (BoundingBox.java:48-75,114-121).
struct BoundingBox {
    int32_t minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};
    constexpr bool operator==(const BoundingBox&) const = default;

    static constexpr BoundingBox fromCorners(const BlockPos& a, const BlockPos& b) noexcept {
        return BoundingBox{
            imin(a.x, b.x), imin(a.y, b.y), imin(a.z, b.z),
            (a.x > b.x ? a.x : b.x), (a.y > b.y ? a.y : b.y), (a.z > b.z ? a.z : b.z)};
    }

    // BoundingBox.java:114-121 — intersects(BoundingBox): X, then Z, then Y.
    constexpr bool intersects(const BoundingBox& o) const noexcept {
        return maxX >= o.minX && minX <= o.maxX
            && maxZ >= o.minZ && minZ <= o.maxZ
            && maxY >= o.minY && minY <= o.maxY;
    }
};

// net.minecraft.world.level.block.Rotation ordinals (Rotation.java:18-21).
enum class Rotation : int32_t {
    NONE = 0, CLOCKWISE_90 = 1, CLOCKWISE_180 = 2, COUNTERCLOCKWISE_90 = 3
};
// net.minecraft.world.level.block.Mirror ordinals (Mirror.java).
enum class Mirror : int32_t { NONE = 0, LEFT_RIGHT = 1, FRONT_BACK = 2 };

// StructureTemplate.transform(BlockPos, Mirror, Rotation, BlockPos pivot)
// — StructureTemplate.java:528-556. Pure int rotation/mirror about a pivot.
// (Already gated as StructureTransforms; reproduced inline so this geometry
// header is self-contained and re-certified by the GT against the real method.)
constexpr BlockPos transform(const BlockPos& pos, Mirror mirror, Rotation rotation,
                             const BlockPos& pivot) noexcept {
    int32_t x = pos.x;
    int32_t y = pos.y;
    int32_t z = pos.z;
    bool wasMirrored = true;
    switch (mirror) {
        case Mirror::LEFT_RIGHT: z = isub(0, z); break;   // z = -z
        case Mirror::FRONT_BACK: x = isub(0, x); break;   // x = -x
        default: wasMirrored = false; break;
    }
    int32_t pivotX = pivot.x;
    int32_t pivotZ = pivot.z;
    switch (rotation) {
        case Rotation::COUNTERCLOCKWISE_90:
            // (pivotX - pivotZ + z, y, pivotX + pivotZ - x)
            return BlockPos{iadd(isub(pivotX, pivotZ), z), y, isub(iadd(pivotX, pivotZ), x)};
        case Rotation::CLOCKWISE_90:
            // (pivotX + pivotZ - z, y, pivotZ - pivotX + x)
            return BlockPos{isub(iadd(pivotX, pivotZ), z), y, iadd(isub(pivotZ, pivotX), x)};
        case Rotation::CLOCKWISE_180:
            // (pivotX + pivotX - x, y, pivotZ + pivotZ - z)
            return BlockPos{isub(iadd(pivotX, pivotX), x), y, isub(iadd(pivotZ, pivotZ), z)};
        default:
            // NONE: mirrored copy if a mirror applied, else the original pos.
            return wasMirrored ? BlockPos{x, y, z} : pos;
    }
}

// ------------------------------------------------------------------ (A) -----
// OceanRuinPieces.allPositions(random, origin) — OceanRuinPieces.java:199-210.
//
// Each of the eight list entries is
//   origin.offset(baseX + Mth.nextInt(random, loX, hiX), 0,
//                 baseZ + Mth.nextInt(random, loZ, hiZ))
// where Mth.nextInt(random, lo, hi) = lo + random.nextInt(hi-lo+1)  (Mth.java:145-146;
// every (lo<hi) here so the >= branch never triggers). The draws happen in list
// order, X before Z within each entry — 16 random.nextInt(bound) calls total.
//
// This gate owns ALL the algorithm constants (the per-axis base offsets and the
// lo/hi bounds, transcribed from OceanRuinPieces.java:201-208). The test feeds
// only the 16 RAW random.nextInt(bound) returns the real RNG produced, in order;
// we add back lo + base ourselves. So the Java GT replicates none of this logic —
// it just records RNG and calls the REAL allPositions for the authoritative
// positions, which this must reproduce bit-for-bit.

// base offset per axis (the "+/-16" / "0" literals), one (x,z) pair per position.
inline constexpr std::array<int32_t, 8> kBaseX{-16, -16, -16, 0, 0, 16, 16, 16};
inline constexpr std::array<int32_t, 8> kBaseZ{ 16,   0, -16, 16, -16, 16, 0, -16};
// Mth.nextInt low bound per axis.
inline constexpr std::array<int32_t, 8> kLoX{1, 1, 1, 1, 1, 1, 1, 1};
inline constexpr std::array<int32_t, 8> kLoZ{1, 1, 4, 1, 4, 3, 1, 4};

struct ClusterCandidateDraws {
    // The 16 RAW random.nextInt(bound) results, in emission order:
    //   p0.x, p0.z, p1.x, p1.z, ... p7.x, p7.z
    std::array<int32_t, 16> raw{};
};

inline std::array<BlockPos, 8> allPositions(const BlockPos& origin,
                                            const ClusterCandidateDraws& d) noexcept {
    std::array<BlockPos, 8> out{};
    for (int i = 0; i < 8; ++i) {
        // Mth.nextInt result = lo + rawDraw  (Mth.java:146).
        int32_t mthX = iadd(kLoX[i], d.raw[2 * i]);
        int32_t mthZ = iadd(kLoZ[i], d.raw[2 * i + 1]);
        int32_t dx = iadd(kBaseX[i], mthX);
        int32_t dz = iadd(kBaseZ[i], mthZ);
        out[i] = origin.offset(dx, 0, dz);
    }
    return out;
}

// ------------------------------------------------------------------ (B) -----
// addClusterRuins parent-box geometry — OceanRuinPieces.java:176-181.
struct ParentBoxResult {
    BlockPos parentPos;
    BlockPos parentCorner;
    BoundingBox parentBB;
    BlockPos parentBottomLeft;
};

inline ParentBoxResult parentBox(const BlockPos& p, Rotation rotation) noexcept {
    BlockPos parentPos{p.x, 90, p.z};
    BlockPos parentCorner =
        transform(BlockPos{15, 0, 15}, Mirror::NONE, rotation, BlockPos{0, 0, 0})
            .offset(parentPos.x, parentPos.y, parentPos.z);
    BoundingBox parentBB = BoundingBox::fromCorners(parentPos, parentCorner);
    BlockPos parentBottomLeft{imin(parentPos.x, parentCorner.x), parentPos.y,
                              imin(parentPos.z, parentCorner.z)};
    return {parentPos, parentCorner, parentBB, parentBottomLeft};
}

// Per-candidate fit test — OceanRuinPieces.java:190-192.
//   nextCorner = transform((5,0,6), NONE, nextRotation, ZERO).offset(pos)
//   nextBB     = fromCorners(pos, nextCorner)
//   fits       = !nextBB.intersects(parentBB)
struct CandidateFitResult {
    BlockPos nextCorner;
    BoundingBox nextBB;
    bool fits;
};

inline CandidateFitResult candidateFit(const BlockPos& pos, Rotation nextRotation,
                                       const BoundingBox& parentBB) noexcept {
    BlockPos nextCorner =
        transform(BlockPos{5, 0, 6}, Mirror::NONE, nextRotation, BlockPos{0, 0, 0})
            .offset(pos.x, pos.y, pos.z);
    BoundingBox nextBB = BoundingBox::fromCorners(pos, nextCorner);
    bool fits = !nextBB.intersects(parentBB);
    return {nextCorner, nextBB, fits};
}

} // namespace mc::levelgen::structure::oceanruin
