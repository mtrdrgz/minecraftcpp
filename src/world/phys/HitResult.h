#pragma once

// 1:1 port of net.minecraft.world.phys.HitResult (+ BlockHitResult,
// EntityHitResult) — Minecraft Java Edition 26.1.2. These are the immutable
// results of a ray-trace: the abstract base carries a Vec3 `location`, the two
// concrete subclasses add a hit Direction/BlockPos (block) or an entity hit.
//
// Java sources:
//   net/minecraft/world/phys/HitResult.java
//   net/minecraft/world/phys/BlockHitResult.java
//   net/minecraft/world/phys/EntityHitResult.java
//
// Pure geometry only — all the arithmetic is double subtraction/sum, so the port
// is exactly bit-for-bit. Certified by hit_result_parity.
//
// PORTED:
//   HitResult.distanceTo(Vec3)   — the geometry of HitResult.distanceTo(Entity)
//                                  (location.x - e.getX() …) taking the entity
//                                  position as a Vec3. Equivalent to
//                                  location.distanceToSqr(pos). (HitResult.java:12-17)
//   HitResult.getLocation()                                  (HitResult.java:21-23)
//   HitResult.Type enum  MISS=0, BLOCK=1, ENTITY=2           (HitResult.java:25-29)
//   BlockHitResult.miss / ctor / private ctor               (BlockHitResult.java:13-34)
//   BlockHitResult.withDirection / withPosition / hitBorder  (BlockHitResult.java:36-46)
//   BlockHitResult.getBlockPos / getDirection / getType      (BlockHitResult.java:48-59)
//   BlockHitResult.isInside / isWorldBorderHit               (BlockHitResult.java:61-67)
//   EntityHitResult.getLocation() (inherited; the Vec3 it is constructed with)
//
// NOT ported (need un-ported deps, hard-absent — never stubbed):
//   HitResult.distanceTo(Entity)  — needs net.minecraft.world.entity.Entity
//                                   (getX/getY/getZ). The geometry is ported as
//                                   distanceTo(Vec3) above.
//   EntityHitResult.getEntity() / Entity ctor — needs Entity.

#include "Direction.h"
#include "Vec3.h"
#include "../../core/Math.h" // mc::BlockPos

namespace mc {

// Java: HitResult.Type — ordinals MISS=0, BLOCK=1, ENTITY=2 (HitResult.java:25-29).
enum class HitResultType : int32_t { MISS = 0, BLOCK = 1, ENTITY = 2 };

// Java: net.minecraft.world.phys.HitResult — abstract base (HitResult.java).
struct HitResult {
    Vec3 location;

    constexpr explicit HitResult(const Vec3& location_) : location(location_) {}

    // Java: HitResult.getLocation() (HitResult.java:21-23).
    constexpr const Vec3& getLocation() const { return location; }

    // Java: HitResult.distanceTo(Entity) geometry (HitResult.java:12-17) — the
    // double subtraction/sum-of-squares against the entity position, taken as a
    // Vec3. Bit-identical to location.distanceToSqr(pos).
    double distanceTo(const Vec3& pos) const {
        double xd = location.x - pos.x;
        double yd = location.y - pos.y;
        double zd = location.z - pos.z;
        return xd * xd + yd * yd + zd * zd;
    }
};

// Java: net.minecraft.world.phys.BlockHitResult (BlockHitResult.java).
struct BlockHitResult : HitResult {
    Direction direction;
    BlockPos blockPos;
    bool miss;
    bool inside;
    bool worldBorderHit;

    // Java: private BlockHitResult(boolean miss, Vec3, Direction, BlockPos,
    // boolean inside, boolean worldBorderHit) (BlockHitResult.java:25-34).
    constexpr BlockHitResult(bool miss_, const Vec3& location_, Direction direction_,
                             const BlockPos& blockPos_, bool inside_, bool worldBorderHit_)
        : HitResult(location_), direction(direction_), blockPos(blockPos_),
          miss(miss_), inside(inside_), worldBorderHit(worldBorderHit_) {}

    // Java: public BlockHitResult(Vec3, Direction, BlockPos, boolean inside)
    // -> this(false, location, direction, pos, inside, false) (BlockHitResult.java:17-19).
    constexpr BlockHitResult(const Vec3& location_, Direction direction_,
                             const BlockPos& pos, bool inside_)
        : BlockHitResult(false, location_, direction_, pos, inside_, false) {}

    // Java: public BlockHitResult(Vec3, Direction, BlockPos, boolean inside,
    // boolean worldBorderHit) (BlockHitResult.java:21-23).
    constexpr BlockHitResult(const Vec3& location_, Direction direction_,
                             const BlockPos& pos, bool inside_, bool worldBorderHit_)
        : BlockHitResult(false, location_, direction_, pos, inside_, worldBorderHit_) {}

    // Java: static BlockHitResult.miss(Vec3, Direction, BlockPos) (BlockHitResult.java:13-15).
    static constexpr BlockHitResult missOf(const Vec3& location_, Direction direction_,
                                           const BlockPos& pos) {
        return BlockHitResult(true, location_, direction_, pos, false, false);
    }

    // Java: BlockHitResult.withDirection(Direction) (BlockHitResult.java:36-38).
    constexpr BlockHitResult withDirection(Direction direction_) const {
        return BlockHitResult(miss, location, direction_, blockPos, inside, worldBorderHit);
    }

    // Java: BlockHitResult.withPosition(BlockPos) (BlockHitResult.java:40-42).
    constexpr BlockHitResult withPosition(const BlockPos& blockPos_) const {
        return BlockHitResult(miss, location, direction, blockPos_, inside, worldBorderHit);
    }

    // Java: BlockHitResult.hitBorder() (BlockHitResult.java:44-46).
    constexpr BlockHitResult hitBorder() const {
        return BlockHitResult(miss, location, direction, blockPos, inside, true);
    }

    // Java: BlockHitResult.getBlockPos() (BlockHitResult.java:48-50).
    constexpr const BlockPos& getBlockPos() const { return blockPos; }

    // Java: BlockHitResult.getDirection() (BlockHitResult.java:52-54).
    constexpr Direction getDirection() const { return direction; }

    // Java: BlockHitResult.getType() — miss ? MISS : BLOCK (BlockHitResult.java:56-59).
    constexpr HitResultType getType() const {
        return miss ? HitResultType::MISS : HitResultType::BLOCK;
    }

    // Java: BlockHitResult.isInside() (BlockHitResult.java:61-63).
    constexpr bool isInside() const { return inside; }

    // Java: BlockHitResult.isWorldBorderHit() (BlockHitResult.java:65-67).
    constexpr bool isWorldBorderHit() const { return worldBorderHit; }
};

// Java: net.minecraft.world.phys.EntityHitResult (EntityHitResult.java). Only the
// location-carrying surface is ported; getEntity()/Entity ctor need Entity.
struct EntityHitResult : HitResult {
    // Java: public EntityHitResult(Entity, Vec3 location) -> super(location)
    // (EntityHitResult.java:12-15). We accept only the location.
    constexpr explicit EntityHitResult(const Vec3& location_) : HitResult(location_) {}

    // Java: EntityHitResult.getType() — always ENTITY (EntityHitResult.java:21-24).
    constexpr HitResultType getType() const { return HitResultType::ENTITY; }
};

} // namespace mc
