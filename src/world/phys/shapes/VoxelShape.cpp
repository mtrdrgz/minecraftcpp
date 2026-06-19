#include "world/phys/shapes/VoxelShape.h"

#include <cmath>
#include <limits>
#include <stdexcept>

#include "world/phys/shapes/JavaMath.h"
#include "world/phys/shapes/Shapes.h"

// Port of VoxelShape.java + ArrayVoxelShape.java + SliceShape.java method bodies
// (Minecraft Java Edition 26.1.2). Every block cites the Java lines it mirrors.

namespace mc {

namespace {
constexpr double POS_INF = std::numeric_limits<double>::infinity();
constexpr double NEG_INF = -std::numeric_limits<double>::infinity();
} // namespace

// Java: VoxelShape.java:29-32.
double VoxelShape::min(Axis axis) const {
    int32_t i = shape->firstFull(axis);
    return i >= shape->getSize(axis) ? POS_INF : get(axis, i);
}

// Java: VoxelShape.java:34-37.
double VoxelShape::max(Axis axis) const {
    int32_t i = shape->lastFull(axis);
    return i <= 0 ? NEG_INF : get(axis, i);
}

// Java: VoxelShape.java:39-52.
AABB VoxelShape::bounds() const {
    if (isEmpty()) {
        // Java: Util.pauseInIde(new UnsupportedOperationException(...)) then throw.
        throw std::logic_error("No bounds for empty shape.");
    }
    return AABB(min(Axis::X), min(Axis::Y), min(Axis::Z),
                max(Axis::X), max(Axis::Y), max(Axis::Z));
}

// Java: VoxelShape.java:54-65.
VoxelShapePtr VoxelShape::singleEncompassing() const {
    return isEmpty() ? Shapes::empty()
                     : Shapes::box(min(Axis::X), min(Axis::Y), min(Axis::Z),
                                   max(Axis::X), max(Axis::Y), max(Axis::Z));
}

// Java: VoxelShape.java:85-94.
VoxelShapePtr VoxelShape::move(double dx, double dy, double dz) const {
    if (isEmpty()) return Shapes::empty();
    return std::make_shared<ArrayVoxelShape>(
        shape,
        std::make_shared<OffsetDoubleList>(getCoords(Axis::X), dx),
        std::make_shared<OffsetDoubleList>(getCoords(Axis::Y), dy),
        std::make_shared<OffsetDoubleList>(getCoords(Axis::Z), dz));
}

// Java: VoxelShape.java:96-100.
VoxelShapePtr VoxelShape::optimize() const {
    VoxelShapePtr result = Shapes::empty();
    forAllBoxes([&result](double x1, double y1, double z1, double x2, double y2, double z2) {
        result = Shapes::joinUnoptimized(result, Shapes::box(x1, y1, z1, x2, y2, z2),
                                         BooleanOps::OR);
    });
    return result;
}

// Java: VoxelShape.java:102-115.
void VoxelShape::forAllEdges(const DoubleLineConsumer& consumer) const {
    shape->forAllEdges(
        [&](int32_t xi1, int32_t yi1, int32_t zi1, int32_t xi2, int32_t yi2, int32_t zi2) {
            consumer(get(Axis::X, xi1), get(Axis::Y, yi1), get(Axis::Z, zi1),
                     get(Axis::X, xi2), get(Axis::Y, yi2), get(Axis::Z, zi2));
        },
        true);
}

// Java: VoxelShape.java:117-128.
void VoxelShape::forAllBoxes(const DoubleLineConsumer& consumer) const {
    DoubleListPtr xCoords = getCoords(Axis::X);
    DoubleListPtr yCoords = getCoords(Axis::Y);
    DoubleListPtr zCoords = getCoords(Axis::Z);
    shape->forAllBoxes(
        [&](int32_t xi1, int32_t yi1, int32_t zi1, int32_t xi2, int32_t yi2, int32_t zi2) {
            consumer(xCoords->getDouble(xi1), yCoords->getDouble(yi1), zCoords->getDouble(zi1),
                     xCoords->getDouble(xi2), yCoords->getDouble(yi2), zCoords->getDouble(zi2));
        },
        true);
}

// Java: VoxelShape.java:130-134.
std::vector<AABB> VoxelShape::toAabbs() const {
    std::vector<AABB> list;
    forAllBoxes([&list](double x1, double y1, double z1, double x2, double y2, double z2) {
        list.emplace_back(x1, y1, z1, x2, y2, z2);
    });
    return list;
}

// Java: VoxelShape.java:136-143.
double VoxelShape::min(Axis aAxis, double b, double c) const {
    Axis bAxis = axisCycleAxis(AxisCycle::FORWARD, aAxis);
    Axis cAxis = axisCycleAxis(AxisCycle::BACKWARD, aAxis);
    int32_t bi = findIndex(bAxis, b);
    int32_t ci = findIndex(cAxis, c);
    int32_t i = shape->firstFull(aAxis, bi, ci);
    return i >= shape->getSize(aAxis) ? POS_INF : get(aAxis, i);
}

// Java: VoxelShape.java:145-152.
double VoxelShape::max(Axis aAxis, double b, double c) const {
    Axis bAxis = axisCycleAxis(AxisCycle::FORWARD, aAxis);
    Axis cAxis = axisCycleAxis(AxisCycle::BACKWARD, aAxis);
    int32_t bi = findIndex(bAxis, b);
    int32_t ci = findIndex(cAxis, c);
    int32_t i = shape->lastFull(aAxis, bi, ci);
    return i <= 0 ? NEG_INF : get(aAxis, i);
}

// Java: VoxelShape.java:154-156.
int32_t VoxelShape::findIndex(Axis axis, double coord) const {
    return mthBinarySearch(0, shape->getSize(axis) + 1,
                           [&](int32_t index) { return coord < get(axis, index); })
         - 1;
}

// Java: VoxelShape.java:158-177.
std::optional<BlockHitResult> VoxelShape::clip(const glm::dvec3& from, const glm::dvec3& to,
                                               const BlockPos& pos) const {
    if (isEmpty()) return std::nullopt;

    // Java: Vec3 diff = to.subtract(from); diff.lengthSqr() (x*x + y*y + z*z).
    const double diffX = to.x - from.x;
    const double diffY = to.y - from.y;
    const double diffZ = to.z - from.z;
    if (diffX * diffX + diffY * diffY + diffZ * diffZ < 1.0E-7) return std::nullopt;

    // Java: Vec3 testPoint = from.add(diff.scale(0.001)).
    const glm::dvec3 testPoint(from.x + diffX * 0.001, from.y + diffY * 0.001,
                               from.z + diffZ * 0.001);
    if (shape->isFullWide(findIndex(Axis::X, testPoint.x - pos.x),
                          findIndex(Axis::Y, testPoint.y - pos.y),
                          findIndex(Axis::Z, testPoint.z - pos.z))) {
        return BlockHitResult(
            testPoint,
            directionOpposite(directionGetApproximateNearest(diffX, diffY, diffZ)), pos, true);
    }
    return AABB::clip(toAabbs(), from, to, pos);
}

// Java: VoxelShape.java:179-195.
std::optional<glm::dvec3> VoxelShape::closestPointTo(const glm::dvec3& point) const {
    if (isEmpty()) return std::nullopt;

    std::optional<glm::dvec3> closest;
    forAllBoxes([&](double x1, double y1, double z1, double x2, double y2, double z2) {
        double x = mthClamp(point.x, x1, x2);
        double y = mthClamp(point.y, y1, y2);
        double z = mthClamp(point.z, z1, z2);
        // Java: point.distanceToSqr(x, y, z) — Vec3: dx = x - this.x, etc.
        auto distSqr = [&point](double px, double py, double pz) {
            const double dx = px - point.x;
            const double dy = py - point.y;
            const double dz = pz - point.z;
            return dx * dx + dy * dy + dz * dz;
        };
        if (!closest || distSqr(x, y, z) < distSqr(closest->x, closest->y, closest->z)) {
            closest = glm::dvec3(x, y, z);
        }
    });
    return closest; // Java: Optional.of(requireNonNull(...)) — non-null when non-empty
}

// Java: VoxelShape.java:197-214.
VoxelShapePtr VoxelShape::getFaceShape(Direction direction) const {
    if (!isEmpty() && this != Shapes::block().get()) {
        const int ord = static_cast<int>(direction);
        if (faces_) {
            const VoxelShapePtr& cached = (*faces_)[ord];
            if (cached) return cached;
        } else {
            faces_ = std::make_unique<std::array<VoxelShapePtr, 6>>();
        }

        VoxelShapePtr face = calculateFace(direction);
        (*faces_)[ord] = face;
        return face;
    }
    return shared_from_this();
}

// Java: VoxelShape.java:216-230.
VoxelShapePtr VoxelShape::calculateFace(Direction direction) const {
    Axis axis = directionAxis(direction);
    if (isCubeLikeAlong(axis)) return shared_from_this();

    AxisDirection sign = directionAxisDirection(direction);
    int32_t index = findIndex(axis, sign == AxisDirection::POSITIVE ? 0.9999999 : 1.0E-7);
    auto slice = std::make_shared<SliceShape>(shared_from_this(), axis, index);
    if (slice->isEmpty()) return Shapes::empty();
    return slice->isCubeLike() ? Shapes::block() : VoxelShapePtr(slice);
}

// Java: VoxelShape.java:232-240.
bool VoxelShape::isCubeLike() const {
    for (Axis axis : AXIS_VALUES)
        if (!isCubeLikeAlong(axis)) return false;
    return true;
}

// Java: VoxelShape.java:242-245.
bool VoxelShape::isCubeLikeAlong(Axis axis) const {
    DoubleListPtr coords = getCoords(axis);
    return coords->size() == 2 && doubleMathFuzzyEquals(coords->getDouble(0), 0.0, 1.0E-7)
        && doubleMathFuzzyEquals(coords->getDouble(1), 1.0, 1.0E-7);
}

// Java: VoxelShape.java:247-249.
double VoxelShape::collide(Axis axis, const AABB& moving, double distance) const {
    return collideX(axisCycleBetween(axis, Axis::X), moving, distance);
}

// Java: VoxelShape.java:251-306.
double VoxelShape::collideX(AxisCycle transform, const AABB& moving, double distance) const {
    if (isEmpty()) return distance;
    if (std::abs(distance) < 1.0E-7) return 0.0;

    AxisCycle inverse = axisCycleInverse(transform);
    Axis aAxis = axisCycleAxis(inverse, Axis::X);
    Axis bAxis = axisCycleAxis(inverse, Axis::Y);
    Axis cAxis = axisCycleAxis(inverse, Axis::Z);
    double maxA = moving.max(aAxis);
    double minA = moving.min(aAxis);
    int32_t aMin = findIndex(aAxis, minA + 1.0E-7);
    int32_t aMax = findIndex(aAxis, maxA - 1.0E-7);
    int32_t bMin0 = findIndex(bAxis, moving.min(bAxis) + 1.0E-7);
    int32_t bMin = 0 > bMin0 ? 0 : bMin0; // Math.max(0, ...)
    int32_t bMax0 = findIndex(bAxis, moving.max(bAxis) - 1.0E-7) + 1;
    int32_t bMax = shape->getSize(bAxis) < bMax0 ? shape->getSize(bAxis) : bMax0; // Math.min
    int32_t cMin0 = findIndex(cAxis, moving.min(cAxis) + 1.0E-7);
    int32_t cMin = 0 > cMin0 ? 0 : cMin0;
    int32_t cMax0 = findIndex(cAxis, moving.max(cAxis) - 1.0E-7) + 1;
    int32_t cMax = shape->getSize(cAxis) < cMax0 ? shape->getSize(cAxis) : cMax0;
    int32_t aSize = shape->getSize(aAxis);
    if (distance > 0.0) {
        for (int32_t a = aMax + 1; a < aSize; ++a) {
            for (int32_t b = bMin; b < bMax; ++b) {
                for (int32_t c = cMin; c < cMax; ++c) {
                    if (shape->isFullWide(inverse, a, b, c)) {
                        double newDistance = get(aAxis, a) - maxA;
                        if (newDistance >= -1.0E-7) {
                            distance = distance < newDistance ? distance : newDistance; // min
                        }
                        return distance;
                    }
                }
            }
        }
    } else if (distance < 0.0) {
        for (int32_t a = aMin - 1; a >= 0; --a) {
            for (int32_t b = bMin; b < bMax; ++b) {
                for (int32_t c = cMin; c < cMax; ++c) {
                    if (shape->isFullWide(inverse, a, b, c)) {
                        double newDistance = get(aAxis, a + 1) - minA;
                        if (newDistance <= 1.0E-7) {
                            distance = distance > newDistance ? distance : newDistance; // max
                        }
                        return distance;
                    }
                }
            }
        }
    }

    return distance;
}

// ---------------------------------------------------------------------------
// ArrayVoxelShape
// ---------------------------------------------------------------------------

namespace {
// Java: Arrays.copyOf(values, newLength) — truncates or zero-pads.
std::vector<double> arraysCopyOf(std::vector<double> values, int32_t newLength) {
    values.resize(static_cast<size_t>(newLength), 0.0);
    return values;
}
} // namespace

// Java: ArrayVoxelShape.java:14-21.
ArrayVoxelShape::ArrayVoxelShape(DiscreteVoxelShapePtr shapeIn, std::vector<double> xs,
                                 std::vector<double> ys, std::vector<double> zs)
    : ArrayVoxelShape(
          shapeIn,
          std::make_shared<DoubleArrayList>(arraysCopyOf(std::move(xs), shapeIn->getXSize() + 1)),
          std::make_shared<DoubleArrayList>(arraysCopyOf(std::move(ys), shapeIn->getYSize() + 1)),
          std::make_shared<DoubleArrayList>(
              arraysCopyOf(std::move(zs), shapeIn->getZSize() + 1))) {}

// Java: ArrayVoxelShape.java:23-37.
ArrayVoxelShape::ArrayVoxelShape(DiscreteVoxelShapePtr shapeIn, DoubleListPtr xs, DoubleListPtr ys,
                                 DoubleListPtr zs)
    : VoxelShape(std::move(shapeIn)), xs_(std::move(xs)), ys_(std::move(ys)), zs_(std::move(zs)) {
    int32_t xSize = shape->getXSize() + 1;
    int32_t ySize = shape->getYSize() + 1;
    int32_t zSize = shape->getZSize() + 1;
    if (xSize != xs_->size() || ySize != ys_->size() || zSize != zs_->size()) {
        // Java: Util.pauseInIde(new IllegalArgumentException(...)) then throw.
        throw std::invalid_argument(
            "Lengths of point arrays must be consistent with the size of the VoxelShape.");
    }
}

// ---------------------------------------------------------------------------
// SliceShape
// ---------------------------------------------------------------------------

// Java: SliceShape.java:11-15.
SliceShape::SliceShape(VoxelShapePtr delegate, Axis axis, int32_t point)
    : VoxelShape(makeSlice(delegate->shape, axis, point)),
      delegate_(std::move(delegate)),
      axis_(axis) {}

// Java: SliceShape.java:17-27.
DiscreteVoxelShapePtr SliceShape::makeSlice(const DiscreteVoxelShapePtr& delegate, Axis axis,
                                            int32_t point) {
    return std::make_shared<SubShape>(
        delegate,
        axisChoose(axis, point, 0, 0),
        axisChoose(axis, 0, point, 0),
        axisChoose(axis, 0, 0, point),
        axisChoose(axis, point + 1, delegate->xSize, delegate->xSize),
        axisChoose(axis, delegate->ySize, point + 1, delegate->ySize),
        axisChoose(axis, delegate->zSize, delegate->zSize, point + 1));
}

// Java: SliceShape.java:29-32 (static SLICE_COORDS = new CubePointRange(1)).
DoubleListPtr SliceShape::getCoords(Axis axis) const {
    static const DoubleListPtr SLICE_COORDS = std::make_shared<CubePointRange>(1);
    return axis == axis_ ? SLICE_COORDS : delegate_->getCoords(axis);
}

} // namespace mc
