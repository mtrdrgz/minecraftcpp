#pragma once

// 1:1 port of the PURE per-room bounding-box geometry nested in the REAL
// decompiled 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.OceanMonumentPieces
//     -> abstract static class OceanMonumentPiece
//          static int  getRoomIndex(int roomX, int roomY, int roomZ)   [line 1450-1452]
//          static BoundingBox makeBoundingBox(Direction, RoomDefinition,
//                                int roomWidth, int roomHeight, int roomDepth) [line 1473-1497]
//
// These two static helpers are fully self-contained integer geometry: they take
// a room's grid index (roomDefinition.index) and a room footprint in grid units
// and return the world-local BoundingBox of that sub-room within the monument.
// They read ONLY the int `index` of the RoomDefinition — there are NO world
// writes, NO RandomSource, NO registry/datapack, NO BlockState. (The graph-walk
// that *assigns* indices, generateRoomGraph, does use RandomSource and is NOT
// ported here; only the pure index->box math is.)
//
// makeBoundingBox(...) decomposes the index, builds an axis-oriented box via
// StructurePiece.makeBoundingBox(0,0,0,orientation, w*8, h*4, d*8), then
// BoundingBox.move(...)s it by an orientation-dependent offset. That involves:
//   * roomIndex % 5 / (roomIndex / 5 % 5) / (roomIndex / 25)   (Java truncating
//     int division & modulo — for the special wing indices 1001/1002/1003 these
//     produce out-of-grid coords; the function still runs and we replicate it),
//   * StructurePiece.makeBoundingBox's getAxis()==Z width/depth swap,
//   * the negated/offset move deltas e.g. -(roomZ + roomDepth) * 8 + 1.
//
// Certified byte-exact by ocean_monument_room_parity
// (tools/OceanMonumentRoomParity.java drives the REAL OceanMonumentPiece via
// reflection and emits a TSV; this header recomputes and compares).

#include <algorithm>
#include <cstdint>

namespace mc::levelgen::structure::oceanmonument {

// Java int arithmetic wraps on overflow (two's complement). C++ signed overflow
// is UB, so route every add/sub/mul through uint32_t and reinterpret, keeping the
// port byte-identical to Java for every input (the GT battery stays in a benign
// range, but using these helpers makes the port correct by construction).
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t isub(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b));
}
constexpr int32_t imul(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) * static_cast<uint32_t>(b));
}
constexpr int32_t ineg(int32_t a) noexcept {
    return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
}

// ── net.minecraft.core.Direction (ordinals match Java declaration order) ─────
enum class Direction : int32_t { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

enum class Axis : int32_t { X = 0, Y = 1, Z = 2 };

// Direction.getAxis() — Direction.java (DOWN/UP=Y, NORTH/SOUTH=Z, WEST/EAST=X).
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

// ── net.minecraft.world.level.levelgen.structure.BoundingBox ─────────────────
// Minimal int-field box: the ctor's inverted-bounds normalization (BoundingBox
// .java:55-63) and the in-place move() (BoundingBox.java:184-192). makeBoundingBox
// always produces non-inverted bounds here, but the normalization is replicated
// faithfully so the type matches the real class.
struct BoundingBox {
    int32_t minX{}, minY{}, minZ{}, maxX{}, maxY{}, maxZ{};

    constexpr BoundingBox() = default;
    constexpr BoundingBox(int x0, int y0, int z0, int x1, int y1, int z1) noexcept
        : minX(x0), minY(y0), minZ(z0), maxX(x1), maxY(y1), maxZ(z1) {
        if (maxX < minX || maxY < minY || maxZ < minZ) {
            int nMinX = std::min(x0, x1), nMinY = std::min(y0, y1), nMinZ = std::min(z0, z1);
            int nMaxX = std::max(x0, x1), nMaxY = std::max(y0, y1), nMaxZ = std::max(z0, z1);
            minX = nMinX; minY = nMinY; minZ = nMinZ;
            maxX = nMaxX; maxY = nMaxY; maxZ = nMaxZ;
        }
    }

    constexpr bool operator==(const BoundingBox&) const = default;

    // BoundingBox.move(int,int,int) — BoundingBox.java:184-192 (mutates in place).
    constexpr BoundingBox& move(int dx, int dy, int dz) noexcept {
        minX = iadd(minX, dx); minY = iadd(minY, dy); minZ = iadd(minZ, dz);
        maxX = iadd(maxX, dx); maxY = iadd(maxY, dy); maxZ = iadd(maxZ, dz);
        return *this;
    }
};

// StructurePiece.makeBoundingBox(x,y,z,Direction,width,height,depth) — the axis
// version (StructurePiece.java:72-78). Java evaluates "x + width - 1" left-to-
// right as (x + width) - 1, all wrapping.
constexpr BoundingBox structurePieceMakeBoundingBox(int x, int y, int z, Direction direction,
                                                    int width, int height, int depth) noexcept {
    return directionAxis(direction) == Axis::Z
        ? BoundingBox(x, y, z, isub(iadd(x, width), 1), isub(iadd(y, height), 1), isub(iadd(z, depth), 1))
        : BoundingBox(x, y, z, isub(iadd(x, depth), 1), isub(iadd(y, height), 1), isub(iadd(z, width), 1));
}

// OceanMonumentPiece.getRoomIndex(roomX,roomY,roomZ) — OceanMonumentPieces.java
// :1450-1452:  roomY * 25 + roomZ * 5 + roomX.
constexpr int getRoomIndex(int roomX, int roomY, int roomZ) noexcept {
    return iadd(iadd(imul(roomY, 25), imul(roomZ, 5)), roomX);
}

// OceanMonumentPiece.makeBoundingBox(Direction, RoomDefinition, roomWidth,
// roomHeight, roomDepth) — OceanMonumentPieces.java:1473-1497.
//
// `roomIndex` is the RoomDefinition.index. Java decomposition (truncating int
// division/modulo, evaluated left-to-right):
//   roomX = roomIndex % 5
//   roomZ = roomIndex / 5 % 5     == (roomIndex / 5) % 5
//   roomY = roomIndex / 25
constexpr BoundingBox makeRoomBoundingBox(Direction orientation, int roomIndex,
                                          int roomWidth, int roomHeight, int roomDepth) noexcept {
    int roomX = roomIndex % 5;          // Java % truncates toward zero (C++ matches)
    int roomZ = (roomIndex / 5) % 5;
    int roomY = roomIndex / 25;

    BoundingBox box = structurePieceMakeBoundingBox(
        0, 0, 0, orientation, imul(roomWidth, 8), imul(roomHeight, 4), imul(roomDepth, 8));

    switch (orientation) {
        case Direction::NORTH:
            box.move(imul(roomX, 8), imul(roomY, 4),
                     iadd(ineg(imul(iadd(roomZ, roomDepth), 8)), 1));
            break;
        case Direction::SOUTH:
            box.move(imul(roomX, 8), imul(roomY, 4), imul(roomZ, 8));
            break;
        case Direction::WEST:
            box.move(iadd(ineg(imul(iadd(roomZ, roomDepth), 8)), 1),
                     imul(roomY, 4), imul(roomX, 8));
            break;
        case Direction::EAST:
        default:
            box.move(imul(roomZ, 8), imul(roomY, 4), imul(roomX, 8));
            break;
    }
    return box;
}

} // namespace mc::levelgen::structure::oceanmonument
