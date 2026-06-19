#pragma once
#include <glm/glm.hpp>

#include "core/Math.h"          // mc::BlockPos
#include "world/phys/Direction.h"

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/phys/BlockHitResult.java (+ the `location` field
// and Type enum of its base net/minecraft/world/phys/HitResult.java), Minecraft
// Java Edition 26.1.2 — the subset needed by AABB.clip(Iterable, ...) and
// VoxelShape.clip.
//
// Java fields (BlockHitResult.java:7-11 + HitResult location):
//   Vec3 location; Direction direction; BlockPos blockPos;
//   boolean miss; boolean inside; boolean worldBorderHit;
//
// NOT ported: withDirection/withPosition/hitBorder helpers (trivial copies,
// no callers yet), the entity HitResult subclass.
// ---------------------------------------------------------------------------

namespace mc {

class BlockHitResult {
public:
    glm::dvec3 location;   // HitResult.location
    Direction direction;   // BlockHitResult.java:7
    BlockPos blockPos;     // BlockHitResult.java:8
    bool miss;             // BlockHitResult.java:9
    bool inside;           // BlockHitResult.java:10
    bool worldBorderHit;   // BlockHitResult.java:11

    // Java: HitResult.Type (MISS/BLOCK relevant here).
    enum class Type { MISS, BLOCK };

    // Java: BlockHitResult(Vec3, Direction, BlockPos, boolean inside) —
    // BlockHitResult.java:17-19 (miss=false, worldBorderHit=false).
    BlockHitResult(const glm::dvec3& location, Direction direction, const BlockPos& blockPos,
                   bool inside) noexcept
        : location(location), direction(direction), blockPos(blockPos),
          miss(false), inside(inside), worldBorderHit(false) {}

    // Java: static BlockHitResult.miss(Vec3, Direction, BlockPos) — BlockHitResult.java:13-15.
    static BlockHitResult missResult(const glm::dvec3& location, Direction direction,
                                     const BlockPos& blockPos) noexcept {
        BlockHitResult r(location, direction, blockPos, false);
        r.miss = true;
        return r;
    }

    // Java: getType() — BlockHitResult.java:56-59.
    Type getType() const noexcept { return miss ? Type::MISS : Type::BLOCK; }
    // Java: getDirection()/getBlockPos()/isInside()/isWorldBorderHit().
    Direction getDirection() const noexcept { return direction; }
    const BlockPos& getBlockPos() const noexcept { return blockPos; }
    bool isInside() const noexcept { return inside; }
    bool isWorldBorderHit() const noexcept { return worldBorderHit; }
};

} // namespace mc
