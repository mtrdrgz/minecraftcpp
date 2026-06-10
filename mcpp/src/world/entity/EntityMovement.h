#pragma once

// 1:1 port of the pure (world-independent) movement-input math from
// net.minecraft.world.entity.Entity (26.1.2): getInputVector / moveRelative — the
// transform from a local WASD input vector + yaw into a world-space delta. Built on
// the certified Vec3 + Mth (table cos/sin). Certified by entity_movement_parity.

#include "../phys/Vec3.h"
#include "../level/levelgen/Mth.h"

namespace mc {

namespace mth = mc::levelgen::mth;

// Entity.getInputVector(Vec3 input, float speed, float yRot) — Entity.java:1671-1681.
inline Vec3 getInputVector(const Vec3& input, float speed, float yRot) {
    double length = input.lengthSqr();
    if (length < 1.0E-7) return Vec3{0, 0, 0};
    Vec3 movement = (length > 1.0 ? input.normalize() : input).scale(speed);
    float sin = mth::sin((double)(yRot * static_cast<float>(3.141592653589793 / 180.0)));
    float cos = mth::cos((double)(yRot * static_cast<float>(3.141592653589793 / 180.0)));
    return {movement.x * cos - movement.z * sin, movement.y, movement.z * cos + movement.x * sin};
}

} // namespace mc
