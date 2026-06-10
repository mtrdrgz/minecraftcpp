#pragma once

// 1:1 port of the world-independent collision-resolution core of
// net.minecraft.world.entity.Entity (26.1.2): collideWithShapes (axis-ordered slide
// of a moving AABB against a list of VoxelShapes) + Direction.axisStepOrder. Built
// entirely on already-certified pieces: Shapes.collide (voxel_shapes_parity), AABB
// (aabb_parity), Vec3 (vec3_parity). Certified by entity_collision_parity.

#include "world/phys/Vec3.h"
#include "world/phys/AABB.h"
#include "world/phys/shapes/Shapes.h"

#include <array>
#include <cmath>
#include <vector>

namespace mc {

// Direction.axisStepOrder(Vec3) — Direction.java: |x|<|z| ? YZX : YXZ. (Axis X=0,Y=1,Z=2)
inline std::array<Axis, 3> axisStepOrder(const Vec3& m) {
    if (std::fabs(m.x) < std::fabs(m.z)) return {Axis::Y, Axis::Z, Axis::X};
    return {Axis::Y, Axis::X, Axis::Z};
}

// Entity.collideWithShapes — Entity.java:1168-1186.
inline Vec3 collideWithShapes(const Vec3& movement, const AABB& boundingBox,
                              const std::vector<VoxelShapePtr>& shapes) {
    if (shapes.empty()) return movement;
    Vec3 resolved{0, 0, 0};
    for (Axis axis : axisStepOrder(movement)) {
        double axisMovement = movement.get(static_cast<int>(axis));
        if (axisMovement != 0.0) {
            double collision = Shapes::collide(axis, boundingBox.move(resolved.x, resolved.y, resolved.z), shapes, axisMovement);
            resolved = resolved.with(static_cast<int>(axis), collision);
        }
    }
    return resolved;
}

} // namespace mc
