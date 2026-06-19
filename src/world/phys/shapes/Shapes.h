#pragma once
#include <cstdint>
#include <vector>

#include "world/phys/AABB.h"
#include "world/phys/Direction.h"
#include "world/phys/shapes/BooleanOp.h"
#include "world/phys/shapes/IndexMerger.h"
#include "world/phys/shapes/VoxelShape.h"

// ---------------------------------------------------------------------------
// Port of net/minecraft/world/phys/shapes/Shapes.java (26.1.2).
//
// Naming notes: Java `or` is a C++ alternative-operator token -> or_();
// Java INFINITY collides with the <cmath> macro -> infinity().
//
// NOT ported (hard-absent until their dependencies land, never stubbed):
//   rotate(VoxelShape, OctahedralGroup[, Vec3]) and the rotateHorizontal/
//   rotateAllAxis/rotateAll/rotateAttachFace block-shape-table builders
//   (Shapes.java:318-462) — they need com/mojang/math/OctahedralGroup.
// ---------------------------------------------------------------------------

namespace mc {

class Shapes {
public:
    static constexpr double EPSILON = 1.0E-7;      // Shapes.java:21
    static constexpr double BIG_EPSILON = 1.0E-6;  // Shapes.java:22

    // Java: Shapes.empty()/block()/INFINITY — canonical singletons; identity matters.
    static const VoxelShapePtr& empty();   // Shapes.java:37-46
    static const VoxelShapePtr& block();   // Shapes.java:23-27, 48-50
    static const VoxelShapePtr& infinity(); // Shapes.java:29-36

    // Java: Shapes.box — Shapes.java:52-58 (validates min <= max).
    static VoxelShapePtr box(double minX, double minY, double minZ,
                             double maxX, double maxY, double maxZ);

    // Java: Shapes.create(double...) — Shapes.java:60-96.
    static VoxelShapePtr create(double minX, double minY, double minZ,
                                double maxX, double maxY, double maxZ);
    // Java: Shapes.create(AABB) — Shapes.java:98-100.
    static VoxelShapePtr create(const AABB& aabb);

    // Java: Shapes.findBits — Shapes.java:102-120.
    static int32_t findBits(double min, double max);

    // Java: Shapes.lcm — Shapes.java:122-124.
    static int64_t lcm(int32_t first, int32_t second);

    // Java: Shapes.or — Shapes.java:126-132 (`or` is a C++ token).
    static VoxelShapePtr or_(const VoxelShapePtr& first, const VoxelShapePtr& second);

    // Java: Shapes.join — Shapes.java:134-136.
    static VoxelShapePtr join(const VoxelShapePtr& first, const VoxelShapePtr& second,
                              BooleanOp op);
    // Java: Shapes.joinUnoptimized — Shapes.java:138-172.
    static VoxelShapePtr joinUnoptimized(const VoxelShapePtr& first, const VoxelShapePtr& second,
                                         BooleanOp op);
    // Java: Shapes.joinIsNotEmpty — Shapes.java:174-214.
    static bool joinIsNotEmpty(const VoxelShapePtr& first, const VoxelShapePtr& second,
                               BooleanOp op);

    // Java: Shapes.collide(axis, AABB, Iterable<VoxelShape>, double) — Shapes.java:231-241.
    static double collide(Axis axis, const AABB& moving, const std::vector<VoxelShapePtr>& shapes,
                          double distance);

    // Java: Shapes.blockOccludes — Shapes.java:243-260.
    static bool blockOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder,
                              Direction direction);
    // Java: Shapes.mergedFaceOccludes — Shapes.java:262-284.
    static bool mergedFaceOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder,
                                   Direction direction);
    // Java: Shapes.faceShapeOccludes — Shapes.java:286-292.
    static bool faceShapeOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder);

    // Java: Shapes.equal — Shapes.java:370-372.
    static bool equal(const VoxelShapePtr& first, const VoxelShapePtr& second);

    // Java: Shapes.createIndexMerger — Shapes.java:294-316.
    static IndexMergerPtr createIndexMerger(int32_t cost, const DoubleListPtr& first,
                                            const DoubleListPtr& second, bool firstOnlyMatters,
                                            bool secondOnlyMatters);

private:
    // Java: private static boolean joinIsNotEmpty(IndexMerger x3, shapes, op) — :216-229.
    static bool joinIsNotEmpty(const IndexMerger& xMerger, const IndexMerger& yMerger,
                               const IndexMerger& zMerger, const DiscreteVoxelShape& first,
                               const DiscreteVoxelShape& second, BooleanOp op);
};

} // namespace mc
