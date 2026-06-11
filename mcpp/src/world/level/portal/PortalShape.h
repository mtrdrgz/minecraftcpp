#pragma once

// 1:1 port of the pure static math of net.minecraft.world.level.portal.PortalShape
// (Minecraft Java Edition 26.1.2) — specifically getRelativePosition(...), the
// helper that maps an entity's world position onto the [0,1]^2 (+forward offset)
// relative coordinate inside the largest empty rectangle of a nether portal. This
// is the math that survives an overworld<->nether teleport so the entity comes out
// the far portal at the same relative spot. It is PURE: it touches no Level, no
// entity tick state, no registry — only the FoundRectangle's three ints, the
// portal Direction.Axis, the entity's double position, and the entity's
// (float) width/height. Certified bit-for-bit by portal_shape_parity.
//
// Java source (PortalShape.java:198-223):
//   public static Vec3 getRelativePosition(BlockUtil.FoundRectangle largest,
//       Direction.Axis axis, Vec3 position, EntityDimensions dimensions) {
//     double width  = (double)largest.axis1Size - dimensions.width();
//     double height = (double)largest.axis2Size - dimensions.height();
//     BlockPos bottomMin = largest.minCorner;
//     double relativeRight;
//     if (width > 0.0) {
//        double bottomStart = bottomMin.get(axis) + dimensions.width() / 2.0;
//        relativeRight = Mth.clamp(Mth.inverseLerp(position.get(axis) - bottomStart, 0.0, width), 0.0, 1.0);
//     } else { relativeRight = 0.5; }
//     double relativeUp;
//     if (height > 0.0) {
//        Direction.Axis heightAxis = Direction.Axis.Y;
//        relativeUp = Mth.clamp(Mth.inverseLerp(position.get(heightAxis) - bottomMin.get(heightAxis), 0.0, height), 0.0, 1.0);
//     } else { relativeUp = 0.0; }
//     Direction.Axis forwardAxis = axis == Direction.Axis.X ? Direction.Axis.Z : Direction.Axis.X;
//     double relativeForward = position.get(forwardAxis) - (bottomMin.get(forwardAxis) + 0.5);
//     return new Vec3(relativeRight, relativeUp, relativeForward);
//   }
//
// 1:1 float-vs-double traps reproduced below:
//   * `(double)axis1Size - dimensions.width()`: axis1Size is int, width() is float;
//     the float is widened to double FIRST, then the subtraction is double — so we
//     must subtract the float's exact (double-widened) value, never re-typed.
//   * `bottomMin.get(axis) + dimensions.width() / 2.0`: int + (float/double). The
//     `width()/2.0` divides a float by a double literal (float widens to double),
//     and only THEN is the int added — all in double.
//   * `bottomMin.get(forwardAxis) + 0.5`: int + double computed BEFORE the outer
//     subtraction; keep that grouping.
//   * Mth.clamp(double) is `value<min?min:Math.min(value,max)` (NaN-asymmetric);
//     Mth.inverseLerp(double) is `(value-min)/(max-min)`. Both reused from Mth.h.
//
// NOT ported here (world/level coupled — hard absent, never stubbed): the whole
// portal-frame scan (findAnyShape/calculate*/getDistanceUntil*), createPortalBlocks,
// isComplete, findCollisionFreePosition (needs ServerLevel.findFreePosition).

#include "world/level/levelgen/Mth.h"
#include "world/phys/Direction.h"
#include "world/phys/Vec3.h"

namespace mc::portal {

namespace mth = mc::levelgen::mth;

// Mirror of net.minecraft.util.BlockUtil.FoundRectangle (BlockUtil.java:137-147):
// a minCorner BlockPos plus two int side lengths. Only the integer components of
// minCorner that get(axis) can select are needed, so we store x/y/z directly.
struct FoundRectangle {
    int minCornerX = 0;
    int minCornerY = 0;
    int minCornerZ = 0;
    int axis1Size = 0;
    int axis2Size = 0;

    // Vec3i.get(Direction.Axis) — Vec3i.java:237-239 (axis.choose(x, y, z)).
    int minCornerGet(mc::Axis axis) const {
        return mc::axisChoose(axis, minCornerX, minCornerY, minCornerZ);
    }
};

// PortalShape.getRelativePosition(FoundRectangle, Direction.Axis, Vec3, EntityDimensions).
// `entityWidth`/`entityHeight` are the entity dimensions' float width()/height().
inline Vec3 getRelativePosition(const FoundRectangle& largestRectangleAround,
                                mc::Axis axis,
                                const Vec3& position,
                                float entityWidth,
                                float entityHeight) {
    // (double)int - float : the float widens to double, subtraction in double.
    double width = static_cast<double>(largestRectangleAround.axis1Size) - static_cast<double>(entityWidth);
    double height = static_cast<double>(largestRectangleAround.axis2Size) - static_cast<double>(entityHeight);

    double relativeRight;
    if (width > 0.0) {
        // int + (float / 2.0): width()/2.0 is float-widened-to-double / double, then + int.
        double bottomStart = static_cast<double>(largestRectangleAround.minCornerGet(axis))
                           + static_cast<double>(entityWidth) / 2.0;
        relativeRight = mth::clamp(
            mth::inverseLerp(position.get(static_cast<int>(axis)) - bottomStart, 0.0, width),
            0.0, 1.0);
    } else {
        relativeRight = 0.5;
    }

    double relativeUp;
    if (height > 0.0) {
        const mc::Axis heightAxis = mc::Axis::Y;
        relativeUp = mth::clamp(
            mth::inverseLerp(
                position.get(static_cast<int>(heightAxis))
                    - static_cast<double>(largestRectangleAround.minCornerGet(heightAxis)),
                0.0, height),
            0.0, 1.0);
    } else {
        relativeUp = 0.0;
    }

    const mc::Axis forwardAxis = axis == mc::Axis::X ? mc::Axis::Z : mc::Axis::X;
    // int + 0.5 computed first, then the outer double subtraction.
    double relativeForward = position.get(static_cast<int>(forwardAxis))
                           - (static_cast<double>(largestRectangleAround.minCornerGet(forwardAxis)) + 0.5);
    return Vec3(relativeRight, relativeUp, relativeForward);
}

} // namespace mc::portal
