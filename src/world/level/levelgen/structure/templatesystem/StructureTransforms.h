#pragma once

// 1:1 port of the pure transform math that underpins ALL template/jigsaw
// structure placement in Minecraft 26.1.2:
//   net.minecraft.world.level.block.Rotation            (getRotated, rotate)
//   net.minecraft.world.level.block.Mirror              (mirror, getRotation)
//   net.minecraft.world.level.levelgen.structure.BoundingBox
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//       (static transform / getZeroPositionWithTransform / getBoundingBox)
//
// Everything here is pure integer/double arithmetic copied verbatim from the
// decompiled source; no registries, no world. It is certified byte-exact by the
// structure_transform_parity gate (tools/StructureTransformsParity.java).
//
// Position type: net.minecraft.core.Vec3i / BlockPos -> mc::levelgen::structure::Vec3i.
// Direction (incl. getClockWise/getCounterClockWise) is reused from world/phys/Direction.h.

#include "../../../../../world/phys/Direction.h"

#include <algorithm>
#include <cstdint>

namespace mc::levelgen::structure {

// net.minecraft.core.Vec3i (BlockPos is a Vec3i subclass; same layout we need).
// Guarded: BoundingBox.h also defines mc::levelgen::structure::Vec3i (a smaller
// version without the offset/getX/Y/Z methods). Whichever header is included
// first wins; this version is a superset so it works for both consumers.
#ifndef MC_LEVELGEN_STRUCTURE_VEC3I_DEFINED
#define MC_LEVELGEN_STRUCTURE_VEC3I_DEFINED
struct Vec3i {
    int32_t x = 0, y = 0, z = 0;
    constexpr bool operator==(const Vec3i&) const = default;
    // Vec3i.offset(int,int,int) — Vec3i.java
    constexpr Vec3i offset(int dx, int dy, int dz) const noexcept { return {x + dx, y + dy, z + dz}; }
    constexpr int getX() const noexcept { return x; }
    constexpr int getY() const noexcept { return y; }
    constexpr int getZ() const noexcept { return z; }
};
#endif
using BlockPos = Vec3i;

inline constexpr Vec3i kBlockPosZero{0, 0, 0};

// net.minecraft.world.phys.Vec3 (only the fields the transform needs).
struct Vec3d {
    double x = 0, y = 0, z = 0;
    constexpr bool operator==(const Vec3d&) const = default;
};

// ── net.minecraft.world.level.block.Rotation (ordinals match Java) ───────────
enum class Rotation : int32_t { NONE = 0, CLOCKWISE_90 = 1, CLOCKWISE_180 = 2, COUNTERCLOCKWISE_90 = 3 };

// Rotation.getRotated(Rotation) — Rotation.java:38-84.
constexpr Rotation rotationGetRotated(Rotation self, Rotation rot) noexcept {
    switch (rot) {
        case Rotation::CLOCKWISE_90:
            switch (self) {
                case Rotation::NONE:                return Rotation::CLOCKWISE_90;
                case Rotation::CLOCKWISE_90:        return Rotation::CLOCKWISE_180;
                case Rotation::CLOCKWISE_180:       return Rotation::COUNTERCLOCKWISE_90;
                case Rotation::COUNTERCLOCKWISE_90: return Rotation::NONE;
            }
            return self;
        case Rotation::CLOCKWISE_180:
            switch (self) {
                case Rotation::NONE:                return Rotation::CLOCKWISE_180;
                case Rotation::CLOCKWISE_90:        return Rotation::COUNTERCLOCKWISE_90;
                case Rotation::CLOCKWISE_180:       return Rotation::NONE;
                case Rotation::COUNTERCLOCKWISE_90: return Rotation::CLOCKWISE_90;
            }
            return self;
        case Rotation::COUNTERCLOCKWISE_90:
            switch (self) {
                case Rotation::NONE:                return Rotation::COUNTERCLOCKWISE_90;
                case Rotation::CLOCKWISE_90:        return Rotation::NONE;
                case Rotation::CLOCKWISE_180:       return Rotation::CLOCKWISE_90;
                case Rotation::COUNTERCLOCKWISE_90: return Rotation::CLOCKWISE_180;
            }
            return self;
        default: // NONE
            return self;
    }
}

// Rotation.rotate(Direction) — Rotation.java:90-101.
// Fully-qualified ::mc::Direction: when BoundingBox.h (which defines its own
// mc::levelgen::structure::Direction) is in the same TU, the unqualified name
// would resolve to that one and break. Pin to ::mc::Direction explicitly.
constexpr ::mc::Direction rotationRotate(Rotation self, ::mc::Direction direction) noexcept {
    if (::mc::directionAxis(direction) == ::mc::Axis::Y) return direction;
    switch (self) {
        case Rotation::CLOCKWISE_90:        return ::mc::directionGetClockWise(direction);
        case Rotation::CLOCKWISE_180:       return ::mc::directionOpposite(direction);
        case Rotation::COUNTERCLOCKWISE_90: return ::mc::directionGetCounterClockWise(direction);
        default:                            return direction;
    }
}

// Rotation.rotate(int rotation, int steps) — Rotation.java:103-110.
constexpr int rotationRotateInt(Rotation self, int rotation, int steps) noexcept {
    switch (self) {
        case Rotation::CLOCKWISE_90:        return (rotation + steps / 4) % steps;
        case Rotation::CLOCKWISE_180:       return (rotation + steps / 2) % steps;
        case Rotation::COUNTERCLOCKWISE_90: return (rotation + steps * 3 / 4) % steps;
        default:                            return rotation;
    }
}

// ── net.minecraft.world.level.block.Mirror (ordinals match Java) ─────────────
enum class Mirror : int32_t { NONE = 0, LEFT_RIGHT = 1, FRONT_BACK = 2 };

// Mirror.mirror(int rotation, int steps) — Mirror.java:28-39.
constexpr int mirrorMirrorInt(Mirror self, int rotation, int steps) noexcept {
    int halfSteps = steps / 2;
    int correctedRotation = rotation > halfSteps ? rotation - steps : rotation;
    switch (self) {
        case Mirror::LEFT_RIGHT: return (halfSteps - correctedRotation + steps) % steps;
        case Mirror::FRONT_BACK: return (steps - correctedRotation) % steps;
        default:                 return rotation;
    }
}

// Mirror.getRotation(Direction) — Mirror.java:41-44.
constexpr Rotation mirrorGetRotation(Mirror self, ::mc::Direction value) noexcept {
    ::mc::Axis axis = ::mc::directionAxis(value);
    return ((self != Mirror::LEFT_RIGHT || axis != ::mc::Axis::Z) &&
            (self != Mirror::FRONT_BACK || axis != ::mc::Axis::X))
               ? Rotation::NONE
               : Rotation::CLOCKWISE_180;
}

// Mirror.mirror(Direction) — Mirror.java:46-52.
constexpr ::mc::Direction mirrorMirror(Mirror self, ::mc::Direction direction) noexcept {
    if (self == Mirror::FRONT_BACK && ::mc::directionAxis(direction) == ::mc::Axis::X) return ::mc::directionOpposite(direction);
    if (self == Mirror::LEFT_RIGHT && ::mc::directionAxis(direction) == ::mc::Axis::Z) return ::mc::directionOpposite(direction);
    return direction;
}

// ── net.minecraft.world.level.levelgen.structure.BoundingBox ─────────────────
// Guarded: BoundingBox.h also defines mc::levelgen::structure::BoundingBox
// (with extra methods like getYSpan/getCenter). Whichever is included first
// wins; the two are layout-compatible (same first six int32_t fields).
#ifndef MC_LEVELGEN_STRUCTURE_BOUNDINGBOX_DEFINED
#define MC_LEVELGEN_STRUCTURE_BOUNDINGBOX_DEFINED
struct BoundingBox {
    int32_t minX, minY, minZ, maxX, maxY, maxZ;

    // BoundingBox(int,int,int,int,int,int) — BoundingBox.java:48-64 (normalizes inverted bounds).
    constexpr BoundingBox(int aMinX, int aMinY, int aMinZ, int aMaxX, int aMaxY, int aMaxZ) noexcept
        : minX(aMinX), minY(aMinY), minZ(aMinZ), maxX(aMaxX), maxY(aMaxY), maxZ(aMaxZ) {
        if (maxX < minX || maxY < minY || maxZ < minZ) {
            minX = std::min(aMinX, aMaxX); minY = std::min(aMinY, aMaxY); minZ = std::min(aMinZ, aMaxZ);
            maxX = std::max(aMinX, aMaxX); maxY = std::max(aMinY, aMaxY); maxZ = std::max(aMinZ, aMaxZ);
        }
    }
    // BoundingBox(BlockPos) — BoundingBox.java:44-46.
    constexpr explicit BoundingBox(const Vec3i& p) noexcept : BoundingBox(p.x, p.y, p.z, p.x, p.y, p.z) {}

    constexpr bool operator==(const BoundingBox&) const = default;

    // BoundingBox.fromCorners(Vec3i, Vec3i) — BoundingBox.java:66-75.
    static constexpr BoundingBox fromCorners(const Vec3i& a, const Vec3i& b) noexcept {
        return BoundingBox(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z),
                           std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z));
    }

    // BoundingBox.orientBox(...) — BoundingBox.java:81-104.
    static constexpr BoundingBox orientBox(int footX, int footY, int footZ, int offX, int offY, int offZ,
                                           int width, int height, int depth, Direction direction) noexcept {
        switch (direction) {
            case Direction::NORTH:
                return BoundingBox(footX + offX, footY + offY, footZ - depth + 1 + offZ,
                                   footX + width - 1 + offX, footY + height - 1 + offY, footZ + offZ);
            case Direction::WEST:
                return BoundingBox(footX - depth + 1 + offZ, footY + offY, footZ + offX,
                                   footX + offZ, footY + height - 1 + offY, footZ + width - 1 + offX);
            case Direction::EAST:
                return BoundingBox(footX + offZ, footY + offY, footZ + offX,
                                   footX + depth - 1 + offZ, footY + height - 1 + offY, footZ + width - 1 + offX);
            case Direction::SOUTH:
            default:
                return BoundingBox(footX + offX, footY + offY, footZ + offZ,
                                   footX + width - 1 + offX, footY + height - 1 + offY, footZ + depth - 1 + offZ);
        }
    }

    // BoundingBox.intersects(BoundingBox) — BoundingBox.java:114-121.
    constexpr bool intersects(const BoundingBox& o) const noexcept {
        return maxX >= o.minX && minX <= o.maxX && maxZ >= o.minZ && minZ <= o.maxZ && maxY >= o.minY && minY <= o.maxY;
    }
    // BoundingBox.intersects(int,int,int,int) — BoundingBox.java:123-125.
    constexpr bool intersects(int oMinX, int oMinZ, int oMaxX, int oMaxZ) const noexcept {
        return maxX >= oMinX && minX <= oMaxX && maxZ >= oMinZ && minZ <= oMaxZ;
    }

    // BoundingBox.encapsulate(BoundingBox) — BoundingBox.java:151-159 (mutating).
    constexpr BoundingBox& encapsulate(const BoundingBox& o) noexcept {
        minX = std::min(minX, o.minX); minY = std::min(minY, o.minY); minZ = std::min(minZ, o.minZ);
        maxX = std::max(maxX, o.maxX); maxY = std::max(maxY, o.maxY); maxZ = std::max(maxZ, o.maxZ);
        return *this;
    }
    // BoundingBox.encapsulate(BlockPos) — BoundingBox.java:173-181 (mutating).
    constexpr BoundingBox& encapsulate(const Vec3i& p) noexcept {
        minX = std::min(minX, p.x); minY = std::min(minY, p.y); minZ = std::min(minZ, p.z);
        maxX = std::max(maxX, p.x); maxY = std::max(maxY, p.y); maxZ = std::max(maxZ, p.z);
        return *this;
    }
    // BoundingBox.encapsulating(a,b) — BoundingBox.java:161-170.
    static constexpr BoundingBox encapsulating(const BoundingBox& a, const BoundingBox& b) noexcept {
        return BoundingBox(std::min(a.minX, b.minX), std::min(a.minY, b.minY), std::min(a.minZ, b.minZ),
                           std::max(a.maxX, b.maxX), std::max(a.maxY, b.maxY), std::max(a.maxZ, b.maxZ));
    }

    // BoundingBox.move(int,int,int) — BoundingBox.java:184-192 (mutating).
    constexpr BoundingBox& move(int dx, int dy, int dz) noexcept {
        minX += dx; minY += dy; minZ += dz; maxX += dx; maxY += dy; maxZ += dz; return *this;
    }
    constexpr BoundingBox& move(const Vec3i& a) noexcept { return move(a.x, a.y, a.z); }
    // BoundingBox.moved(int,int,int) — BoundingBox.java:199-201.
    constexpr BoundingBox moved(int dx, int dy, int dz) const noexcept {
        return BoundingBox(minX + dx, minY + dy, minZ + dz, maxX + dx, maxY + dy, maxZ + dz);
    }
    // BoundingBox.inflatedBy(int,int,int) — BoundingBox.java:207-211.
    constexpr BoundingBox inflatedBy(int ix, int iy, int iz) const noexcept {
        return BoundingBox(minX - ix, minY - iy, minZ - iz, maxX + ix, maxY + iy, maxZ + iz);
    }
    constexpr BoundingBox inflatedBy(int all) const noexcept { return inflatedBy(all, all, all); }

    // BoundingBox.isInside(int,int,int) — BoundingBox.java:217-219.
    constexpr bool isInside(int x, int y, int z) const noexcept {
        return x >= minX && x <= maxX && z >= minZ && z <= maxZ && y >= minY && y <= maxY;
    }
    constexpr bool isInside(const Vec3i& p) const noexcept { return isInside(p.x, p.y, p.z); }

    // BoundingBox.getLength()/spans/center — BoundingBox.java:221-239.
    constexpr Vec3i getLength() const noexcept { return {maxX - minX, maxY - minY, maxZ - minZ}; }
    constexpr int getXSpan() const noexcept { return maxX - minX + 1; }
    constexpr int getYSpan() const noexcept { return maxY - minY + 1; }
    constexpr int getZSpan() const noexcept { return maxZ - minZ + 1; }
    constexpr Vec3i getCenter() const noexcept {
        return {minX + (maxX - minX + 1) / 2, minY + (maxY - minY + 1) / 2, minZ + (maxZ - minZ + 1) / 2};
    }
};
#endif  // MC_LEVELGEN_STRUCTURE_BOUNDINGBOX_DEFINED

// ── StructureTemplate static transforms ──────────────────────────────────────

// StructureTemplate.transform(BlockPos, Mirror, Rotation, BlockPos pivot) — StructureTemplate.java:528-556.
constexpr BlockPos structureTransform(const BlockPos& pos, Mirror mirror, Rotation rotation, const BlockPos& pivot) noexcept {
    int x = pos.x, y = pos.y, z = pos.z;
    bool wasMirrored = true;
    switch (mirror) {
        case Mirror::LEFT_RIGHT: z = -z; break;
        case Mirror::FRONT_BACK: x = -x; break;
        default: wasMirrored = false;
    }
    int pivotX = pivot.x, pivotZ = pivot.z;
    switch (rotation) {
        case Rotation::COUNTERCLOCKWISE_90: return {pivotX - pivotZ + z, y, pivotX + pivotZ - x};
        case Rotation::CLOCKWISE_90:        return {pivotX + pivotZ - z, y, pivotZ - pivotX + x};
        case Rotation::CLOCKWISE_180:       return {pivotX + pivotX - x, y, pivotZ + pivotZ - z};
        default:                            return wasMirrored ? BlockPos{x, y, z} : pos;
    }
}

// StructureTemplate.transform(Vec3, Mirror, Rotation, BlockPos pivot) — StructureTemplate.java:558-586.
constexpr Vec3d structureTransform(const Vec3d& pos, Mirror mirror, Rotation rotation, const BlockPos& pivot) noexcept {
    double x = pos.x, y = pos.y, z = pos.z;
    bool wasMirrored = true;
    switch (mirror) {
        case Mirror::LEFT_RIGHT: z = 1.0 - z; break;
        case Mirror::FRONT_BACK: x = 1.0 - x; break;
        default: wasMirrored = false;
    }
    int pivotX = pivot.x, pivotZ = pivot.z;
    switch (rotation) {
        case Rotation::COUNTERCLOCKWISE_90: return {pivotX - pivotZ + z, y, pivotX + pivotZ + 1 - x};
        case Rotation::CLOCKWISE_90:        return {pivotX + pivotZ + 1 - z, y, pivotZ - pivotX + x};
        case Rotation::CLOCKWISE_180:       return {pivotX + pivotX + 1 - x, y, pivotZ + pivotZ + 1 - z};
        default:                            return wasMirrored ? Vec3d{x, y, z} : pos;
    }
}

// StructureTemplate.getZeroPositionWithTransform(BlockPos, Mirror, Rotation, int, int) — StructureTemplate.java:592-613.
constexpr BlockPos getZeroPositionWithTransform(const BlockPos& zeroPos, Mirror mirror, Rotation rotation,
                                                int sizeX, int sizeZ) noexcept {
    sizeX--;
    sizeZ--;
    int mirrorDeltaX = mirror == Mirror::FRONT_BACK ? sizeX : 0;
    int mirrorDeltaZ = mirror == Mirror::LEFT_RIGHT ? sizeZ : 0;
    switch (rotation) {
        case Rotation::COUNTERCLOCKWISE_90: return zeroPos.offset(mirrorDeltaZ, 0, sizeX - mirrorDeltaX);
        case Rotation::CLOCKWISE_90:        return zeroPos.offset(sizeZ - mirrorDeltaZ, 0, mirrorDeltaX);
        case Rotation::CLOCKWISE_180:       return zeroPos.offset(sizeX - mirrorDeltaX, 0, sizeZ - mirrorDeltaZ);
        default:                            return zeroPos.offset(mirrorDeltaX, 0, mirrorDeltaZ); // NONE
    }
}

// StructureTemplate.getBoundingBox(BlockPos, Rotation, BlockPos pivot, Mirror, Vec3i size) — StructureTemplate.java:623-629.
constexpr BoundingBox structureGetBoundingBox(const BlockPos& position, Rotation rotation, const BlockPos& pivot,
                                              Mirror mirror, const Vec3i& size) noexcept {
    Vec3i delta = size.offset(-1, -1, -1);
    BlockPos corner1 = structureTransform(kBlockPosZero, mirror, rotation, pivot);
    BlockPos corner2 = structureTransform(kBlockPosZero.offset(delta.x, delta.y, delta.z), mirror, rotation, pivot);
    return BoundingBox::fromCorners(corner1, corner2).move(position);
}

} // namespace mc::levelgen::structure
