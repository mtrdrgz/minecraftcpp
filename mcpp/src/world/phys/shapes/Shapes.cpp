#include "world/phys/shapes/Shapes.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "world/phys/shapes/JavaMath.h"

// Port of Shapes.java method bodies (Minecraft Java Edition 26.1.2).

namespace mc {

// Java: Shapes.java:23-27 — Util.make(): 1x1x1 discrete shape, fill(0,0,0), CubeVoxelShape.
const VoxelShapePtr& Shapes::block() {
    static const VoxelShapePtr BLOCK = [] {
        auto discrete = std::make_shared<BitSetDiscreteVoxelShape>(1, 1, 1);
        discrete->fill(0, 0, 0);
        return std::make_shared<CubeVoxelShape>(std::move(discrete));
    }();
    return BLOCK;
}

// Java: Shapes.java:37-46 — ArrayVoxelShape over a 0x0x0 grid with coords {0.0}.
const VoxelShapePtr& Shapes::empty() {
    static const VoxelShapePtr EMPTY = std::make_shared<ArrayVoxelShape>(
        std::make_shared<BitSetDiscreteVoxelShape>(0, 0, 0),
        std::make_shared<DoubleArrayList>(std::vector<double>{0.0}),
        std::make_shared<DoubleArrayList>(std::vector<double>{0.0}),
        std::make_shared<DoubleArrayList>(std::vector<double>{0.0}));
    return EMPTY;
}

// Java: Shapes.java:29-36 — INFINITY = box(-inf.., +inf..).
const VoxelShapePtr& Shapes::infinity() {
    static const VoxelShapePtr INFINITE_BOX = [] {
        constexpr double inf = std::numeric_limits<double>::infinity();
        return box(-inf, -inf, -inf, inf, inf, inf);
    }();
    return INFINITE_BOX;
}

// Java: Shapes.java:52-58.
VoxelShapePtr Shapes::box(double minX, double minY, double minZ,
                          double maxX, double maxY, double maxZ) {
    if (!(minX > maxX) && !(minY > maxY) && !(minZ > maxZ)) {
        return create(minX, minY, minZ, maxX, maxY, maxZ);
    }
    throw std::invalid_argument("The min values need to be smaller or equals to the max values");
}

// Java: Shapes.java:60-96.
VoxelShapePtr Shapes::create(double minX, double minY, double minZ,
                             double maxX, double maxY, double maxZ) {
    if (!(maxX - minX < 1.0E-7) && !(maxY - minY < 1.0E-7) && !(maxZ - minZ < 1.0E-7)) {
        int32_t xBits = findBits(minX, maxX);
        int32_t yBits = findBits(minY, maxY);
        int32_t zBits = findBits(minZ, maxZ);
        if (xBits < 0 || yBits < 0 || zBits < 0) {
            return std::make_shared<ArrayVoxelShape>(
                block()->shape,
                std::make_shared<DoubleArrayList>(std::vector<double>{minX, maxX}),
                std::make_shared<DoubleArrayList>(std::vector<double>{minY, maxY}),
                std::make_shared<DoubleArrayList>(std::vector<double>{minZ, maxZ}));
        }

        if (xBits == 0 && yBits == 0 && zBits == 0) {
            return block();
        }

        int32_t xSize = 1 << xBits;
        int32_t ySize = 1 << yBits;
        int32_t zSize = 1 << zBits;
        // Java: (int)Math.round(coord * size).
        auto voxelShape = BitSetDiscreteVoxelShape::withFilledBounds(
            xSize, ySize, zSize,
            static_cast<int32_t>(javaMathRound(minX * xSize)),
            static_cast<int32_t>(javaMathRound(minY * ySize)),
            static_cast<int32_t>(javaMathRound(minZ * zSize)),
            static_cast<int32_t>(javaMathRound(maxX * xSize)),
            static_cast<int32_t>(javaMathRound(maxY * ySize)),
            static_cast<int32_t>(javaMathRound(maxZ * zSize)));
        return std::make_shared<CubeVoxelShape>(std::move(voxelShape));
    }
    return empty();
}

// Java: Shapes.java:98-100.
VoxelShapePtr Shapes::create(const AABB& aabb) {
    return create(aabb.minCorner.x, aabb.minCorner.y, aabb.minCorner.z,
                  aabb.maxCorner.x, aabb.maxCorner.y, aabb.maxCorner.z);
}

// Java: Shapes.java:102-120.
int32_t Shapes::findBits(double min, double max) {
    if (!(min < -1.0E-7) && !(max > 1.0000001)) {
        for (int32_t bits = 0; bits <= 3; ++bits) {
            int32_t intervals = 1 << bits;
            double shMin = min * intervals;
            double shMax = max * intervals;
            // Java: Math.abs(sh - Math.round(sh)) < 1.0E-7 * intervals (long -> double).
            bool foundMin =
                std::abs(shMin - static_cast<double>(javaMathRound(shMin))) < 1.0E-7 * intervals;
            bool foundMax =
                std::abs(shMax - static_cast<double>(javaMathRound(shMax))) < 1.0E-7 * intervals;
            if (foundMin && foundMax) return bits;
        }
        return -1;
    }
    return -1;
}

// Java: Shapes.java:122-124.
int64_t Shapes::lcm(int32_t first, int32_t second) {
    return static_cast<int64_t>(first) * (second / intMathGcd(first, second));
}

// Java: Shapes.java:126-128.
VoxelShapePtr Shapes::or_(const VoxelShapePtr& first, const VoxelShapePtr& second) {
    return join(first, second, BooleanOps::OR);
}

// Java: Shapes.java:134-136.
VoxelShapePtr Shapes::join(const VoxelShapePtr& first, const VoxelShapePtr& second, BooleanOp op) {
    return joinUnoptimized(first, second, op)->optimize();
}

// Java: Shapes.java:138-172.
VoxelShapePtr Shapes::joinUnoptimized(const VoxelShapePtr& first, const VoxelShapePtr& second,
                                      BooleanOp op) {
    if (op.apply(false, false)) {
        // Java: Util.pauseInIde(new IllegalArgumentException()) then throw.
        throw std::invalid_argument("BooleanOp produces from nothing");
    }

    if (first.get() == second.get()) { // Java reference identity: first == second
        return op.apply(true, true) ? first : empty();
    }

    bool firstOnlyMatters = op.apply(true, false);
    bool secondOnlyMatters = op.apply(false, true);
    if (first->isEmpty()) {
        return secondOnlyMatters ? second : empty();
    }
    if (second->isEmpty()) {
        return firstOnlyMatters ? first : empty();
    }

    IndexMergerPtr xMerger = createIndexMerger(1, first->getCoords(Axis::X),
                                               second->getCoords(Axis::X), firstOnlyMatters,
                                               secondOnlyMatters);
    IndexMergerPtr yMerger =
        createIndexMerger(xMerger->size() - 1, first->getCoords(Axis::Y),
                          second->getCoords(Axis::Y), firstOnlyMatters, secondOnlyMatters);
    IndexMergerPtr zMerger = createIndexMerger(
        (xMerger->size() - 1) * (yMerger->size() - 1), first->getCoords(Axis::Z),
        second->getCoords(Axis::Z), firstOnlyMatters, secondOnlyMatters);
    auto voxelShape = BitSetDiscreteVoxelShape::join(*first->shape, *second->shape, *xMerger,
                                                     *yMerger, *zMerger, op);
    // Java: xMerger instanceof DiscreteCubeMerger && ... (Shapes.java:169-171).
    const bool allCube = xMerger->isDiscreteCubeMerger() && yMerger->isDiscreteCubeMerger()
                      && zMerger->isDiscreteCubeMerger();
    if (allCube) return std::make_shared<CubeVoxelShape>(std::move(voxelShape));
    return std::make_shared<ArrayVoxelShape>(std::move(voxelShape), xMerger->getList(),
                                             yMerger->getList(), zMerger->getList());
}

// Java: Shapes.java:174-214.
bool Shapes::joinIsNotEmpty(const VoxelShapePtr& first, const VoxelShapePtr& second,
                            BooleanOp op) {
    if (op.apply(false, false)) {
        throw std::invalid_argument("BooleanOp produces from nothing");
    }

    bool firstEmpty = first->isEmpty();
    bool secondEmpty = second->isEmpty();
    if (!firstEmpty && !secondEmpty) {
        if (first.get() == second.get()) {
            return op.apply(true, true);
        }

        bool firstOnlyMatters = op.apply(true, false);
        bool secondOnlyMatters = op.apply(false, true);

        // Java: for (Direction.Axis axis : AxisCycle.AXIS_VALUES) (= X, Y, Z).
        for (Axis axis : AXIS_VALUES) {
            if (first->max(axis) < second->min(axis) - 1.0E-7) {
                return firstOnlyMatters || secondOnlyMatters;
            }
            if (second->max(axis) < first->min(axis) - 1.0E-7) {
                return firstOnlyMatters || secondOnlyMatters;
            }
        }

        IndexMergerPtr xMerger = createIndexMerger(1, first->getCoords(Axis::X),
                                                   second->getCoords(Axis::X), firstOnlyMatters,
                                                   secondOnlyMatters);
        IndexMergerPtr yMerger =
            createIndexMerger(xMerger->size() - 1, first->getCoords(Axis::Y),
                              second->getCoords(Axis::Y), firstOnlyMatters, secondOnlyMatters);
        IndexMergerPtr zMerger = createIndexMerger(
            (xMerger->size() - 1) * (yMerger->size() - 1), first->getCoords(Axis::Z),
            second->getCoords(Axis::Z), firstOnlyMatters, secondOnlyMatters);
        return joinIsNotEmpty(*xMerger, *yMerger, *zMerger, *first->shape, *second->shape, op);
    }
    return op.apply(!firstEmpty, !secondEmpty);
}

// Java: Shapes.java:216-229.
bool Shapes::joinIsNotEmpty(const IndexMerger& xMerger, const IndexMerger& yMerger,
                            const IndexMerger& zMerger, const DiscreteVoxelShape& first,
                            const DiscreteVoxelShape& second, BooleanOp op) {
    return !xMerger.forMergedIndexes([&](int32_t x1, int32_t x2, int32_t) {
        return yMerger.forMergedIndexes([&](int32_t y1, int32_t y2, int32_t) {
            return zMerger.forMergedIndexes([&](int32_t z1, int32_t z2, int32_t) {
                return !op.apply(first.isFullWide(x1, y1, z1), second.isFullWide(x2, y2, z2));
            });
        });
    });
}

// Java: Shapes.java:231-241.
double Shapes::collide(Axis axis, const AABB& moving, const std::vector<VoxelShapePtr>& shapes,
                       double distance) {
    for (const VoxelShapePtr& shape : shapes) {
        if (std::abs(distance) < 1.0E-7) {
            return 0.0;
        }
        distance = shape->collide(axis, moving, distance);
    }
    return distance;
}

// Java: Shapes.java:243-260.
bool Shapes::blockOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder,
                           Direction direction) {
    if (shape.get() == block().get() && occluder.get() == block().get()) {
        return true;
    }
    if (occluder->isEmpty()) {
        return false;
    }

    Axis axis = directionAxis(direction);
    AxisDirection sign = directionAxisDirection(direction);
    const VoxelShapePtr& first = sign == AxisDirection::POSITIVE ? shape : occluder;
    const VoxelShapePtr& second = sign == AxisDirection::POSITIVE ? occluder : shape;
    BooleanOp op = sign == AxisDirection::POSITIVE ? BooleanOps::ONLY_FIRST
                                                   : BooleanOps::ONLY_SECOND;
    return doubleMathFuzzyEquals(first->max(axis), 1.0, 1.0E-7)
        && doubleMathFuzzyEquals(second->min(axis), 0.0, 1.0E-7)
        && !joinIsNotEmpty(
               std::make_shared<SliceShape>(first, axis, first->shape->getSize(axis) - 1),
               std::make_shared<SliceShape>(second, axis, 0), op);
}

// Java: Shapes.java:262-284.
bool Shapes::mergedFaceOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder,
                                Direction direction) {
    if (shape.get() != block().get() && occluder.get() != block().get()) {
        Axis axis = directionAxis(direction);
        AxisDirection sign = directionAxisDirection(direction);
        VoxelShapePtr first = sign == AxisDirection::POSITIVE ? shape : occluder;
        VoxelShapePtr second = sign == AxisDirection::POSITIVE ? occluder : shape;
        if (!doubleMathFuzzyEquals(first->max(axis), 1.0, 1.0E-7)) {
            first = empty();
        }
        if (!doubleMathFuzzyEquals(second->min(axis), 0.0, 1.0E-7)) {
            second = empty();
        }

        return !joinIsNotEmpty(
            block(),
            joinUnoptimized(
                std::make_shared<SliceShape>(first, axis, first->shape->getSize(axis) - 1),
                std::make_shared<SliceShape>(second, axis, 0), BooleanOps::OR),
            BooleanOps::ONLY_FIRST);
    }
    return true;
}

// Java: Shapes.java:286-292.
bool Shapes::faceShapeOccludes(const VoxelShapePtr& shape, const VoxelShapePtr& occluder) {
    if (shape.get() == block().get() || occluder.get() == block().get()) {
        return true;
    }
    return shape->isEmpty() && occluder->isEmpty()
               ? false
               : !joinIsNotEmpty(block(), joinUnoptimized(shape, occluder, BooleanOps::OR),
                                 BooleanOps::ONLY_FIRST);
}

// Java: Shapes.java:370-372.
bool Shapes::equal(const VoxelShapePtr& first, const VoxelShapePtr& second) {
    return !joinIsNotEmpty(first, second, BooleanOps::NOT_SAME);
}

// Java: Shapes.java:294-316.
IndexMergerPtr Shapes::createIndexMerger(int32_t cost, const DoubleListPtr& first,
                                         const DoubleListPtr& second, bool firstOnlyMatters,
                                         bool secondOnlyMatters) {
    int32_t firstSize = first->size() - 1;
    int32_t secondSize = second->size() - 1;
    // Java: first instanceof CubePointRange && second instanceof CubePointRange.
    if (first->isCubePointRange() && second->isCubePointRange()) {
        int64_t size = lcm(firstSize, secondSize);
        if (static_cast<int64_t>(cost) * size <= 256LL) {
            return std::make_shared<DiscreteCubeMerger>(firstSize, secondSize);
        }
    }

    if (first->getDouble(firstSize) < second->getDouble(0) - 1.0E-7) {
        return std::make_shared<NonOverlappingMerger>(first, second, false);
    }
    if (second->getDouble(secondSize) < first->getDouble(0) - 1.0E-7) {
        return std::make_shared<NonOverlappingMerger>(second, first, true);
    }
    // Java: firstSize == secondSize && Objects.equals(first, second) — fastutil
    // list equals() is element-wise content equality.
    if (firstSize == secondSize && doubleListContentEquals(*first, *second)) {
        return std::make_shared<IdenticalMerger>(first);
    }
    return std::make_shared<IndirectMerger>(*first, *second, firstOnlyMatters, secondOnlyMatters);
}

} // namespace mc
