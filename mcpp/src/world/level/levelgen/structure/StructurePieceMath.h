#pragma once

// 1:1 port of the PURE orientation/offset math in
//   net.minecraft.world.level.levelgen.structure.StructurePiece (26.1.2).
//
// Every structure piece (mineshaft, stronghold, ocean monument, jigsaw pool
// element, ...) maps its local (x,y,z) space into world space through these
// helpers. The math is pure integer arithmetic driven only by the piece's
// BoundingBox and its orientation Direction; there are NO world writes, NO
// RandomSource, NO registry/datapack access here. The methods that touch the
// world (placeBlock, generateBox, createChest, ...) are deliberately NOT ported
// in this header — only the self-contained geometry is.
//
// Ported members (verbatim from StructurePiece.java):
//   makeBoundingBox(x,y,z,dir,width,height,depth)   [lines 72-78]
//   setOrientation(Direction) -> (mirror, rotation) [lines 533-557]
//   getWorldX(x,z)                                   [lines 136-153]
//   getWorldY(y)                                     [lines 155-157]
//   getWorldZ(x,z)                                   [lines 159-176]
//   getLocatorPosition() == BoundingBox.getCenter()  [lines 128-130 / BoundingBox 237-239]
//   isCloseToChunk(ChunkPos,distance)                [lines 122-126]
//
// Dependencies are inlined minimally so the header links with no engine .cpp:
//   - net.minecraft.core.Direction horizontal data2d ids + from2DDataValue
//     (Direction.java:33-38, 159-161, 287-289) — only the bits these methods use.
//   - net.minecraft.world.level.block.{Mirror,Rotation} ordinals.
//   - net.minecraft.world.level.levelgen.structure.BoundingBox int fields +
//     getCenter()/intersects(int,int,int,int) (BoundingBox.java).
//
// Certified byte-exact by structure_piece_math_parity
// (tools/StructurePieceMathParity.java).

#include <array>
#include <cstdint>

namespace mc::levelgen::structure::piece {

// Java int arithmetic wraps on overflow (two's complement); C++ signed overflow
// is UB, so do every add/sub through uint32_t and reinterpret. These helpers
// keep the ports byte-identical to Java even at Integer.MIN/MAX_VALUE.
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
constexpr int32_t ishl4(int32_t a) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) << 4);
}

// ── net.minecraft.world.level.block.Mirror / Rotation (ordinals match Java) ──
enum class Mirror : int32_t { NONE = 0, LEFT_RIGHT = 1, FRONT_BACK = 2 };
enum class Rotation : int32_t { NONE = 0, CLOCKWISE_90 = 1, CLOCKWISE_180 = 2, COUNTERCLOCKWISE_90 = 3 };

// ── net.minecraft.core.Direction (only horizontals are ever used here) ───────
// Enum *ordinals* (declaration order) match Java: DOWN,UP,NORTH,SOUTH,WEST,EAST.
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// Direction.java:33-38 — getAxis() for the six facings.
constexpr Axis directionAxis(Direction d) noexcept {
    switch (d) {
        case Direction::DOWN:
        case Direction::UP:    return Axis::Y;
        case Direction::NORTH:
        case Direction::SOUTH: return Axis::Z;
        case Direction::WEST:
        case Direction::EAST:  return Axis::X;
    }
    return Axis::Y;
}

// Direction.java:33-38 — data2d for horizontals (SOUTH=0,WEST=1,NORTH=2,EAST=3);
// verticals have data2d == -1 in Java. get2DDataValue() returns data2d.
constexpr int directionGet2DDataValue(Direction d) noexcept {
    switch (d) {
        case Direction::SOUTH: return 0;
        case Direction::WEST:  return 1;
        case Direction::NORTH: return 2;
        case Direction::EAST:  return 3;
        default:               return -1; // DOWN/UP
    }
}

// Direction.from2DDataValue(int) — Direction.java:287-289.
//   BY_2D_DATA = horizontals sorted by data2d => [SOUTH, WEST, NORTH, EAST].
//   return BY_2D_DATA[Mth.abs(data % 4)].
// Mth.abs(int) is Math.abs; data%4 in Java truncates toward zero (can be negative
// for negative data), then abs() makes it 0..3.
constexpr Direction directionFrom2DDataValue(int data) noexcept {
    constexpr std::array<Direction, 4> BY_2D_DATA{
        Direction::SOUTH, Direction::WEST, Direction::NORTH, Direction::EAST};
    int m = data % 4;          // C++ % truncates toward zero, same as Java
    if (m < 0) m = -m;         // Math.abs
    return BY_2D_DATA[static_cast<std::size_t>(m)];
}

// ── net.minecraft.world.level.levelgen.structure.BoundingBox (int fields) ────
// Only the pure pieces StructurePiece geometry touches. The inverted-bounds fix
// in the ctor (BoundingBox.java:55-63) IS replicated because makeBoundingBox can
// in principle feed it; getCenter uses raw fields.
struct BoundingBox {
    int32_t minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};

    constexpr BoundingBox() = default;
    constexpr BoundingBox(int x0, int y0, int z0, int x1, int y1, int z1) noexcept {
        minX = x0; minY = y0; minZ = z0; maxX = x1; maxY = y1; maxZ = z1;
        // BoundingBox.java:55-63 — swap if inverted (logging omitted).
        if (maxX < minX || maxY < minY || maxZ < minZ) {
            int nMinX = minX < maxX ? minX : maxX;
            int nMinY = minY < maxY ? minY : maxY;
            int nMinZ = minZ < maxZ ? minZ : maxZ;
            int nMaxX = minX > maxX ? minX : maxX;
            int nMaxY = minY > maxY ? minY : maxY;
            int nMaxZ = minZ > maxZ ? minZ : maxZ;
            minX = nMinX; minY = nMinY; minZ = nMinZ;
            maxX = nMaxX; maxY = nMaxY; maxZ = nMaxZ;
        }
    }

    constexpr bool operator==(const BoundingBox&) const = default;

    // BoundingBox.java:123-125 intersects(minX,minZ,maxX,maxZ).
    constexpr bool intersects(int oMinX, int oMinZ, int oMaxX, int oMaxZ) const noexcept {
        return maxX >= oMinX && minX <= oMaxX && maxZ >= oMinZ && minZ <= oMaxZ;
    }

    // BoundingBox.java:237-239 getCenter() == minX + (maxX - minX + 1) / 2,
    // evaluated exactly in Java's left-to-right order with int wrap, then Java
    // integer division (truncate toward zero, which C++ '/' also does).
    constexpr int centerX() const noexcept { return iadd(minX, iadd(isub(maxX, minX), 1) / 2); }
    constexpr int centerY() const noexcept { return iadd(minY, iadd(isub(maxY, minY), 1) / 2); }
    constexpr int centerZ() const noexcept { return iadd(minZ, iadd(isub(maxZ, minZ), 1) / 2); }
};

struct Vec3i { int32_t x{}, y{}, z{}; constexpr bool operator==(const Vec3i&) const = default; };

// ── StructurePiece pure geometry ─────────────────────────────────────────────
// orientation is nullable in Java; we model "no orientation" with hasOrientation=false.
struct StructurePieceMath {
    BoundingBox boundingBox{};
    bool hasOrientation = false;
    Direction orientation = Direction::NORTH;
    Mirror mirror = Mirror::NONE;
    Rotation rotation = Rotation::NONE;

    // StructurePiece.setOrientation(Direction) — lines 533-557.
    // Pass hasOrientation=false to emulate setOrientation(null).
    constexpr void setOrientation(bool present, Direction d) noexcept {
        hasOrientation = present;
        orientation = d;
        if (!present) {
            rotation = Rotation::NONE;
            mirror = Mirror::NONE;
            return;
        }
        switch (d) {
            case Direction::SOUTH:
                mirror = Mirror::LEFT_RIGHT; rotation = Rotation::NONE; break;
            case Direction::WEST:
                mirror = Mirror::LEFT_RIGHT; rotation = Rotation::CLOCKWISE_90; break;
            case Direction::EAST:
                mirror = Mirror::NONE; rotation = Rotation::CLOCKWISE_90; break;
            default: // NORTH and any vertical
                mirror = Mirror::NONE; rotation = Rotation::NONE; break;
        }
    }

    // StructurePiece.getWorldX(int,int) — lines 136-153.
    constexpr int getWorldX(int x, int z) const noexcept {
        if (!hasOrientation) return x;
        switch (orientation) {
            case Direction::NORTH:
            case Direction::SOUTH: return iadd(boundingBox.minX, x);
            case Direction::WEST:  return isub(boundingBox.maxX, z);
            case Direction::EAST:  return iadd(boundingBox.minX, z);
            default:               return x;
        }
    }

    // StructurePiece.getWorldY(int) — lines 155-157.
    constexpr int getWorldY(int y) const noexcept {
        return !hasOrientation ? y : iadd(y, boundingBox.minY);
    }

    // StructurePiece.getWorldZ(int,int) — lines 159-176.
    constexpr int getWorldZ(int x, int z) const noexcept {
        if (!hasOrientation) return z;
        switch (orientation) {
            case Direction::NORTH: return isub(boundingBox.maxZ, z);
            case Direction::SOUTH: return iadd(boundingBox.minZ, z);
            case Direction::WEST:
            case Direction::EAST:  return iadd(boundingBox.minZ, x);
            default:               return z;
        }
    }

    // StructurePiece.getLocatorPosition() == new BlockPos(boundingBox.getCenter())
    // — lines 128-130.
    constexpr Vec3i getLocatorPosition() const noexcept {
        return {boundingBox.centerX(), boundingBox.centerY(), boundingBox.centerZ()};
    }

    // StructurePiece.isCloseToChunk(ChunkPos,int) — lines 122-126.
    // ChunkPos.getMinBlockX() == chunkX << 4 (SectionPos.sectionToBlockCoord).
    constexpr bool isCloseToChunk(int chunkX, int chunkZ, int distance) const noexcept {
        int cx = ishl4(chunkX);
        int cz = ishl4(chunkZ);
        return boundingBox.intersects(isub(cx, distance), isub(cz, distance),
                                      iadd(iadd(cx, 15), distance), iadd(iadd(cz, 15), distance));
    }
};

// StructurePiece.makeBoundingBox(x,y,z,Direction,width,height,depth) — lines 72-78.
// Java evals "x + width - 1" left-to-right: (x + width) - 1, all wrapping.
constexpr BoundingBox makeBoundingBox(int x, int y, int z, Direction direction,
                                      int width, int height, int depth) noexcept {
    return directionAxis(direction) == Axis::Z
        ? BoundingBox(x, y, z, isub(iadd(x, width), 1), isub(iadd(y, height), 1), isub(iadd(z, depth), 1))
        : BoundingBox(x, y, z, isub(iadd(x, depth), 1), isub(iadd(y, height), 1), isub(iadd(z, width), 1));
}

} // namespace mc::levelgen::structure::piece
