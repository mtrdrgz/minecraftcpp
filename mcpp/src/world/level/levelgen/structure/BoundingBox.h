#pragma once

// 1:1 port of the PURE integer geometry in
//   net.minecraft.world.level.levelgen.structure.BoundingBox (26.1.2).
//
// BoundingBox is the foundational axis-aligned int box used by every structure
// piece (mineshaft, stronghold, ocean monument, jigsaw, ...). Everything ported
// here is self-contained integer arithmetic: NO world writes, NO RandomSource,
// NO registry/datapack, NO GL. The world-touching consumers (StructurePiece
// placement, etc.) live elsewhere; this header is only the geometry.
//
// Ported members (verbatim from BoundingBox.java unless noted):
//   ctor(int x6) + inverted-bounds fix                [48-64]
//   fromCorners(Vec3i, Vec3i)                         [66-75]
//   infinite()                                        [77-79]
//   orientBox(footX..depth, Direction)               [81-104]
//   intersectingChunks() -> ChunkPos stream (as longs) [106-112]
//   intersects(BoundingBox)                          [114-121]
//   intersects(minX,minZ,maxX,maxZ)                  [123-125]
//   encapsulating(a,b) / encapsulate(box) / encapsulate(pos) [150-181, 161-170]
//   move(dx,dy,dz) / moved(dx,dy,dz)                 [184-201]
//   inflatedBy(all) / inflatedBy(x,y,z)              [203-211]
//   isInside(Vec3i) / isInside(x,y,z)                [213-219]
//   getLength()                                      [221-223]
//   getXSpan() / getYSpan() / getZSpan()             [225-235]
//   getCenter()                                      [237-239]
//
// Stream-returning intersectingChunks() is ported as a std::vector<int64_t> of
// packed ChunkPos keys, produced in the SAME iteration order as Java's
// ChunkPos.rangeClosed spliterator (ChunkPos.java:206-236): inner loop advances
// X first toward to.x, then steps Z. The pack is ChunkPos.pack(x,z)
// (ChunkPos.java:76-77). blockToSectionCoord is the ARITHMETIC right shift
// `blockCoord >> 4` (SectionPos.java:80-82) — a signed-shift trap.
//
// The deliberately-unported members of BoundingBox.java are the non-pure /
// serialization / display bits: CODEC, STREAM_CODEC, forAllCorners (consumer),
// encapsulatingPositions/encapsulatingBoxes (Iterable<Optional> wrappers),
// toString/equals/hashCode. None affect worldgen geometry; they are listed here
// as UNPORTED rather than stubbed.
//
// Certified byte-exact by bounding_box_parity (tools/BoundingBoxParity.java).

#include <cstdint>
#include <vector>

namespace mc::levelgen::structure {

// Java int arithmetic wraps on overflow (two's complement); C++ signed overflow
// is UB, so every add/sub goes through uint32_t and is reinterpreted. This keeps
// the port byte-identical to Java even near Integer.MIN/MAX_VALUE.
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
constexpr int32_t imin(int32_t a, int32_t b) noexcept { return a < b ? a : b; }
constexpr int32_t imax(int32_t a, int32_t b) noexcept { return a > b ? a : b; }

// SectionPos.blockToSectionCoord(int) — SectionPos.java:80-82: `blockCoord >> 4`.
// Java `>>` on int is an arithmetic shift; C++ `>>` on a signed value is also
// arithmetic (implementation-defined but arithmetic on every target we build
// for). Negatives floor toward -inf, e.g. (-1) >> 4 == -1.
constexpr int32_t blockToSectionCoord(int32_t blockCoord) noexcept {
    return blockCoord >> 4;
}

// ChunkPos.pack(int x, int z) — ChunkPos.java:76-77:
//   x & 0xFFFFFFFFL | (z & 0xFFFFFFFFL) << 32
constexpr int64_t packChunkPos(int32_t x, int32_t z) noexcept {
    return (static_cast<int64_t>(static_cast<uint32_t>(x)))
         | (static_cast<int64_t>(static_cast<uint32_t>(z)) << 32);
}

// net.minecraft.core.Direction ordinals (declaration order):
// DOWN,UP,NORTH,SOUTH,WEST,EAST. orientBox only switches on the four horizontals.
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// net.minecraft.core.Vec3i (only the three int fields are needed).
struct Vec3i {
    int32_t x{}, y{}, z{};
    constexpr bool operator==(const Vec3i&) const = default;
};

struct BoundingBox {
    int32_t minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};

    constexpr BoundingBox() = default;

    // BoundingBox.java:48-64 — store, then swap if any axis is inverted (the
    // logAndPauseIfInIde call is a no-op for the math and is omitted).
    constexpr BoundingBox(int32_t x0, int32_t y0, int32_t z0,
                          int32_t x1, int32_t y1, int32_t z1) noexcept
        : minX(x0), minY(y0), minZ(z0), maxX(x1), maxY(y1), maxZ(z1) {
        if (maxX < minX || maxY < minY || maxZ < minZ) {
            int32_t nMinX = imin(minX, maxX);
            int32_t nMinY = imin(minY, maxY);
            int32_t nMinZ = imin(minZ, maxZ);
            int32_t nMaxX = imax(minX, maxX);
            int32_t nMaxY = imax(minY, maxY);
            int32_t nMaxZ = imax(minZ, maxZ);
            minX = nMinX; minY = nMinY; minZ = nMinZ;
            maxX = nMaxX; maxY = nMaxY; maxZ = nMaxZ;
        }
    }

    constexpr bool operator==(const BoundingBox&) const = default;

    // BoundingBox.java:66-75 — fromCorners(Vec3i, Vec3i).
    static constexpr BoundingBox fromCorners(const Vec3i& a, const Vec3i& b) noexcept {
        return BoundingBox(imin(a.x, b.x), imin(a.y, b.y), imin(a.z, b.z),
                           imax(a.x, b.x), imax(a.y, b.y), imax(a.z, b.z));
    }

    // BoundingBox.java:77-79 — infinite() = MIN..MAX (no swap needed).
    static constexpr BoundingBox infinite() noexcept {
        return BoundingBox(INT32_MIN, INT32_MIN, INT32_MIN, INT32_MAX, INT32_MAX, INT32_MAX);
    }

    // BoundingBox.java:81-104 — orientBox(...). Java evaluates each corner
    // expression left-to-right with wrapping int arithmetic.
    static constexpr BoundingBox orientBox(int32_t footX, int32_t footY, int32_t footZ,
                                           int32_t offX, int32_t offY, int32_t offZ,
                                           int32_t width, int32_t height, int32_t depth,
                                           Direction direction) noexcept {
        switch (direction) {
            case Direction::NORTH:
                // (footX+offX, footY+offY, footZ-depth+1+offZ,
                //  footX+width-1+offX, footY+height-1+offY, footZ+offZ)
                return BoundingBox(
                    iadd(footX, offX), iadd(footY, offY),
                    iadd(iadd(isub(footZ, depth), 1), offZ),
                    iadd(isub(iadd(footX, width), 1), offX),
                    iadd(isub(iadd(footY, height), 1), offY),
                    iadd(footZ, offZ));
            case Direction::WEST:
                // (footX-depth+1+offZ, footY+offY, footZ+offX,
                //  footX+offZ, footY+height-1+offY, footZ+width-1+offX)
                return BoundingBox(
                    iadd(iadd(isub(footX, depth), 1), offZ),
                    iadd(footY, offY), iadd(footZ, offX),
                    iadd(footX, offZ),
                    iadd(isub(iadd(footY, height), 1), offY),
                    iadd(isub(iadd(footZ, width), 1), offX));
            case Direction::EAST:
                // (footX+offZ, footY+offY, footZ+offX,
                //  footX+depth-1+offZ, footY+height-1+offY, footZ+width-1+offX)
                return BoundingBox(
                    iadd(footX, offZ), iadd(footY, offY), iadd(footZ, offX),
                    iadd(isub(iadd(footX, depth), 1), offZ),
                    iadd(isub(iadd(footY, height), 1), offY),
                    iadd(isub(iadd(footZ, width), 1), offX));
            case Direction::SOUTH:
            default:
                // (footX+offX, footY+offY, footZ+offZ,
                //  footX+width-1+offX, footY+height-1+offY, footZ+depth-1+offZ)
                return BoundingBox(
                    iadd(footX, offX), iadd(footY, offY), iadd(footZ, offZ),
                    iadd(isub(iadd(footX, width), 1), offX),
                    iadd(isub(iadd(footY, height), 1), offY),
                    iadd(isub(iadd(footZ, depth), 1), offZ));
        }
    }

    // BoundingBox.java:106-112 — intersectingChunks(), materialized into a vector
    // of packed ChunkPos keys in ChunkPos.rangeClosed iteration order.
    std::vector<int64_t> intersectingChunks() const {
        int32_t fromX = blockToSectionCoord(minX);
        int32_t fromZ = blockToSectionCoord(minZ);
        int32_t toX = blockToSectionCoord(maxX);
        int32_t toZ = blockToSectionCoord(maxZ);
        // ChunkPos.rangeClosed(from, to) — ChunkPos.java:206-236. from = (fromX,
        // fromZ) (the min chunk), to = (toX, toZ) (the max chunk). Because min<=max
        // here, from.x<to.x and from.z<to.z so both diffs are +1; the spliterator
        // walks x to to.x then resets x=from.x and steps z, ending at (to.x,to.z).
        int32_t xDiff = fromX < toX ? 1 : -1;
        int32_t zDiff = fromZ < toZ ? 1 : -1;
        std::vector<int64_t> out;
        int32_t x = fromX, z = fromZ;
        bool started = false;
        while (true) {
            if (!started) {
                started = true;
                x = fromX; z = fromZ;
            } else if (x == toX) {
                if (z == toZ) break;
                x = fromX; z = z + zDiff;
            } else {
                x = x + xDiff;
            }
            out.push_back(packChunkPos(x, z));
        }
        return out;
    }

    // BoundingBox.java:114-121 — intersects(BoundingBox).
    constexpr bool intersects(const BoundingBox& o) const noexcept {
        return maxX >= o.minX && minX <= o.maxX
            && maxZ >= o.minZ && minZ <= o.maxZ
            && maxY >= o.minY && minY <= o.maxY;
    }

    // BoundingBox.java:123-125 — intersects(minX,minZ,maxX,maxZ).
    constexpr bool intersects(int32_t oMinX, int32_t oMinZ,
                              int32_t oMaxX, int32_t oMaxZ) const noexcept {
        return maxX >= oMinX && minX <= oMaxX && maxZ >= oMinZ && minZ <= oMaxZ;
    }

    // BoundingBox.java:150-159 — encapsulate(BoundingBox) (mutating).
    constexpr BoundingBox& encapsulate(const BoundingBox& o) noexcept {
        minX = imin(minX, o.minX); minY = imin(minY, o.minY); minZ = imin(minZ, o.minZ);
        maxX = imax(maxX, o.maxX); maxY = imax(maxY, o.maxY); maxZ = imax(maxZ, o.maxZ);
        return *this;
    }

    // BoundingBox.java:172-181 — encapsulate(BlockPos) (mutating).
    constexpr BoundingBox& encapsulate(const Vec3i& p) noexcept {
        minX = imin(minX, p.x); minY = imin(minY, p.y); minZ = imin(minZ, p.z);
        maxX = imax(maxX, p.x); maxY = imax(maxY, p.y); maxZ = imax(maxZ, p.z);
        return *this;
    }

    // BoundingBox.java:161-170 — encapsulating(a,b) (new box).
    static constexpr BoundingBox encapsulating(const BoundingBox& a, const BoundingBox& b) noexcept {
        return BoundingBox(imin(a.minX, b.minX), imin(a.minY, b.minY), imin(a.minZ, b.minZ),
                           imax(a.maxX, b.maxX), imax(a.maxY, b.maxY), imax(a.maxZ, b.maxZ));
    }

    // BoundingBox.java:184-192 — move(dx,dy,dz) (mutating, wrapping).
    constexpr BoundingBox& move(int32_t dx, int32_t dy, int32_t dz) noexcept {
        minX = iadd(minX, dx); minY = iadd(minY, dy); minZ = iadd(minZ, dz);
        maxX = iadd(maxX, dx); maxY = iadd(maxY, dy); maxZ = iadd(maxZ, dz);
        return *this;
    }

    // BoundingBox.java:199-201 — moved(dx,dy,dz) (new box, wrapping).
    constexpr BoundingBox moved(int32_t dx, int32_t dy, int32_t dz) const noexcept {
        return BoundingBox(iadd(minX, dx), iadd(minY, dy), iadd(minZ, dz),
                           iadd(maxX, dx), iadd(maxY, dy), iadd(maxZ, dz));
    }

    // BoundingBox.java:203-205 — inflatedBy(all).
    constexpr BoundingBox inflatedBy(int32_t amount) const noexcept {
        return inflatedBy(amount, amount, amount);
    }

    // BoundingBox.java:207-211 — inflatedBy(x,y,z) (subtract from min, add to max).
    constexpr BoundingBox inflatedBy(int32_t ix, int32_t iy, int32_t iz) const noexcept {
        return BoundingBox(isub(minX, ix), isub(minY, iy), isub(minZ, iz),
                           iadd(maxX, ix), iadd(maxY, iy), iadd(maxZ, iz));
    }

    // BoundingBox.java:213-219 — isInside(Vec3i) / isInside(x,y,z).
    constexpr bool isInside(const Vec3i& p) const noexcept { return isInside(p.x, p.y, p.z); }
    constexpr bool isInside(int32_t x, int32_t y, int32_t z) const noexcept {
        return x >= minX && x <= maxX && z >= minZ && z <= maxZ && y >= minY && y <= maxY;
    }

    // BoundingBox.java:221-223 — getLength() (wrapping subtract).
    constexpr Vec3i getLength() const noexcept {
        return {isub(maxX, minX), isub(maxY, minY), isub(maxZ, minZ)};
    }

    // BoundingBox.java:225-235 — getXSpan/getYSpan/getZSpan = max - min + 1 (wrapping).
    constexpr int32_t getXSpan() const noexcept { return iadd(isub(maxX, minX), 1); }
    constexpr int32_t getYSpan() const noexcept { return iadd(isub(maxY, minY), 1); }
    constexpr int32_t getZSpan() const noexcept { return iadd(isub(maxZ, minZ), 1); }

    // BoundingBox.java:237-239 — getCenter() = min + (max - min + 1) / 2.
    // Java integer division truncates toward zero, as does C++ '/'.
    constexpr Vec3i getCenter() const noexcept {
        return {iadd(minX, iadd(isub(maxX, minX), 1) / 2),
                iadd(minY, iadd(isub(maxY, minY), 1) / 2),
                iadd(minZ, iadd(isub(maxZ, minZ), 1) / 2)};
    }
};

} // namespace mc::levelgen::structure
