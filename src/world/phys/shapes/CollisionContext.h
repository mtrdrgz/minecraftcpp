#pragma once
#include <cfloat>

#include "core/Math.h" // mc::BlockPos
#include "world/phys/shapes/VoxelShape.h"

// ---------------------------------------------------------------------------
// Port of (Minecraft Java Edition 26.1.2) — the EMPTY-context subset of:
//   net/minecraft/world/phys/shapes/CollisionContext.java
//   net/minecraft/world/phys/shapes/EntityCollisionContext.java
//
// Ported now (needed before the entity port):
//   CollisionContext.empty() / emptyWithFluidCollisions()
//     -> EntityCollisionContext.Empty.WITHOUT_FLUID_COLLISIONS /
//        WITH_FLUID_COLLISIONS (EntityCollisionContext.java:91-103); the Empty
//     context is EntityCollisionContext(false, false, -Double.MAX_VALUE,
//     ItemStack.EMPTY, alwaysCollideWithFluid, null) with isAbove() returning
//     the default value.
//   isDescending() / isAbove() / alwaysCollideWithFluid() / isPlacement()
//
// NOT ported (hard-absent until Entity/Item/BlockState land — never stubbed
// with a permissive default):
//   CollisionContext.of(Entity[, boolean]) — needs Entity
//   CollisionContext.placementContext(Player)/withPosition(Entity, double)
//   MinecartCollisionContext — needs AbstractMinecart
//   isHoldingItem(Item)      — needs Item/ItemStack
//   canStandOnFluid(...)     — needs FluidState/LivingEntity
//   getCollisionShape(...)   — needs BlockState/CollisionGetter
// ---------------------------------------------------------------------------

namespace mc {

class CollisionContext {
public:
    virtual ~CollisionContext() = default;

    // Java: CollisionContext.isDescending() (CollisionContext.java:59).
    virtual bool isDescending() const = 0;
    // Java: CollisionContext.isAbove(VoxelShape, BlockPos, boolean) (:61).
    virtual bool isAbove(const VoxelShape& shape, const BlockPos& pos,
                         bool defaultValue) const = 0;
    // Java: CollisionContext.alwaysCollideWithFluid() (:65).
    virtual bool alwaysCollideWithFluid() const = 0;
    // Java: CollisionContext.isPlacement() default false (:71-73).
    virtual bool isPlacement() const { return false; }

    // Java: CollisionContext.empty() -> Empty.WITHOUT_FLUID_COLLISIONS (:16-18).
    static const CollisionContext& empty();
    // Java: CollisionContext.emptyWithFluidCollisions() -> Empty.WITH_FLUID_COLLISIONS (:20-22).
    static const CollisionContext& emptyWithFluidCollisions();
};

// Java: EntityCollisionContext.Empty (EntityCollisionContext.java:91-103) — the
// entity-less context: descending=false, placement=false, entityBottom=-Double.MAX_VALUE,
// no held item, no entity; isAbove() returns the passed default.
class EmptyCollisionContext final : public CollisionContext {
public:
    explicit constexpr EmptyCollisionContext(bool alwaysCollideWithFluid) noexcept
        : alwaysCollideWithFluid_(alwaysCollideWithFluid) {}

    bool isDescending() const override { return false; } // super(false, ...)
    bool isAbove(const VoxelShape&, const BlockPos&, bool defaultValue) const override {
        return defaultValue; // EntityCollisionContext.java:99-102
    }
    bool alwaysCollideWithFluid() const override { return alwaysCollideWithFluid_; }

private:
    bool alwaysCollideWithFluid_;
};

inline const CollisionContext& CollisionContext::empty() {
    static const EmptyCollisionContext WITHOUT_FLUID_COLLISIONS{false};
    return WITHOUT_FLUID_COLLISIONS;
}

inline const CollisionContext& CollisionContext::emptyWithFluidCollisions() {
    static const EmptyCollisionContext WITH_FLUID_COLLISIONS{true};
    return WITH_FLUID_COLLISIONS;
}

} // namespace mc
