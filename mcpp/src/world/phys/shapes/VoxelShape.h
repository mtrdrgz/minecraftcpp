#pragma once
#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "core/Math.h" // mc::BlockPos
#include "world/phys/AABB.h"
#include "world/phys/BlockHitResult.h"
#include "world/phys/Direction.h"
#include "world/phys/shapes/DiscreteVoxelShape.h"
#include "world/phys/shapes/DoubleList.h"

// ---------------------------------------------------------------------------
// Port of (Minecraft Java Edition 26.1.2):
//   net/minecraft/world/phys/shapes/VoxelShape.java
//   net/minecraft/world/phys/shapes/ArrayVoxelShape.java
//   net/minecraft/world/phys/shapes/CubeVoxelShape.java
//   net/minecraft/world/phys/shapes/SliceShape.java
//
// VoxelShapes are immutable and always shared_ptr-held (Java object identity —
// `first == second`, `this != Shapes.block()` — maps to pointer identity, so
// the Shapes singletons must be THE canonical instances).
//
// NOT ported from VoxelShape.java (no callers in the collision core yet):
//   move(Vec3i) (no Vec3i type yet) — use move(double,double,double).
// ---------------------------------------------------------------------------

namespace mc {

class VoxelShape;
using VoxelShapePtr = std::shared_ptr<const VoxelShape>;

class VoxelShape : public std::enable_shared_from_this<VoxelShape> {
public:
    // Java: protected final DiscreteVoxelShape shape (VoxelShape.java:22).
    const DiscreteVoxelShapePtr shape;

    virtual ~VoxelShape() = default;

    // Java: Shapes.DoubleLineConsumer (Shapes.java:464-466).
    using DoubleLineConsumer = std::function<void(double, double, double, double, double, double)>;

    // Java: VoxelShape.java:29-32.
    double min(Axis axis) const;
    // Java: VoxelShape.java:34-37.
    double max(Axis axis) const;
    // Java: VoxelShape.java:39-52 (throws on empty shape).
    AABB bounds() const;
    // Java: VoxelShape.java:54-65.
    VoxelShapePtr singleEncompassing() const;

    // Java: public abstract DoubleList getCoords(Direction.Axis) (VoxelShape.java:71).
    virtual DoubleListPtr getCoords(Axis axis) const = 0;

    // Java: VoxelShape.java:73-75.
    bool isEmpty() const { return shape->isEmpty(); }

    // Java: VoxelShape.java:77-79 (Vec3 overload) and :85-94.
    VoxelShapePtr move(const glm::dvec3& delta) const { return move(delta.x, delta.y, delta.z); }
    VoxelShapePtr move(double dx, double dy, double dz) const;

    // Java: VoxelShape.java:96-100.
    VoxelShapePtr optimize() const;

    // Java: VoxelShape.java:102-115.
    void forAllEdges(const DoubleLineConsumer& consumer) const;
    // Java: VoxelShape.java:117-128.
    void forAllBoxes(const DoubleLineConsumer& consumer) const;
    // Java: VoxelShape.java:130-134.
    std::vector<AABB> toAabbs() const;

    // Java: VoxelShape.java:136-143 / :145-152.
    double min(Axis aAxis, double b, double c) const;
    double max(Axis aAxis, double b, double c) const;

    // Java: VoxelShape.java:158-177 (@Nullable BlockHitResult -> optional).
    std::optional<BlockHitResult> clip(const glm::dvec3& from, const glm::dvec3& to,
                                       const BlockPos& pos) const;

    // Java: VoxelShape.java:179-195.
    std::optional<glm::dvec3> closestPointTo(const glm::dvec3& point) const;

    // Java: VoxelShape.java:197-214 (lazily caches the six face shapes).
    VoxelShapePtr getFaceShape(Direction direction) const;

    // Java: VoxelShape.java:247-249.
    double collide(Axis axis, const AABB& moving, double distance) const;

protected:
    // Java: VoxelShape.java:25-27.
    explicit VoxelShape(DiscreteVoxelShapePtr shape) : shape(std::move(shape)) {}

    // Java: VoxelShape.java:67-69.
    double get(Axis axis, int32_t i) const { return getCoords(axis)->getDouble(i); }

    // Java: VoxelShape.java:154-156 (overridden by CubeVoxelShape).
    virtual int32_t findIndex(Axis axis, double coord) const;

    // Java: VoxelShape.java:232-240.
    bool isCubeLike() const;

    // Java: VoxelShape.java:251-306.
    double collideX(AxisCycle transform, const AABB& moving, double distance) const;

private:
    // Java: VoxelShape.java:216-230.
    VoxelShapePtr calculateFace(Direction direction) const;
    // Java: VoxelShape.java:242-245.
    bool isCubeLikeAlong(Axis axis) const;

    // Java: private @Nullable VoxelShape @Nullable [] faces (VoxelShape.java:23).
    mutable std::unique_ptr<std::array<VoxelShapePtr, 6>> faces_;
};

// Java: ArrayVoxelShape.java — explicit coordinate lists per axis.
class ArrayVoxelShape final : public VoxelShape {
public:
    // Java: ArrayVoxelShape.java:14-21 (double[] ctor; Arrays.copyOf pads/truncates
    // each array to size+1).
    ArrayVoxelShape(DiscreteVoxelShapePtr shape, std::vector<double> xs, std::vector<double> ys,
                    std::vector<double> zs);

    // Java: ArrayVoxelShape.java:23-37 (DoubleList ctor; throws on length mismatch).
    ArrayVoxelShape(DiscreteVoxelShapePtr shape, DoubleListPtr xs, DoubleListPtr ys,
                    DoubleListPtr zs);

    // Java: ArrayVoxelShape.java:39-46.
    DoubleListPtr getCoords(Axis axis) const override {
        return axis == Axis::X ? xs_ : (axis == Axis::Y ? ys_ : zs_);
    }

private:
    DoubleListPtr xs_;
    DoubleListPtr ys_;
    DoubleListPtr zs_;
};

// Java: CubeVoxelShape.java — coords are the regular i/size grid.
class CubeVoxelShape final : public VoxelShape {
public:
    explicit CubeVoxelShape(DiscreteVoxelShapePtr shape) : VoxelShape(std::move(shape)) {}

    // Java: CubeVoxelShape.java:12-15.
    DoubleListPtr getCoords(Axis axis) const override {
        return std::make_shared<CubePointRange>(shape->getSize(axis));
    }

protected:
    // Java: CubeVoxelShape.java:17-21 — Mth.floor(Mth.clamp(coord * size, -1.0, size)),
    // replacing the binary search of the base class.
    int32_t findIndex(Axis axis, double coord) const override {
        int32_t size = shape->getSize(axis);
        return mthFloor(mthClamp(coord * size, -1.0, static_cast<double>(size)));
    }
};

// Java: SliceShape.java — a one-cell-thick slice of a delegate shape along an axis.
class SliceShape final : public VoxelShape {
public:
    // Java: SliceShape.java:11-15.
    SliceShape(VoxelShapePtr delegate, Axis axis, int32_t point);

    // Java: SliceShape.java:29-32.
    DoubleListPtr getCoords(Axis axis) const override;

private:
    // Java: SliceShape.java:17-27.
    static DiscreteVoxelShapePtr makeSlice(const DiscreteVoxelShapePtr& delegate, Axis axis,
                                           int32_t point);

    VoxelShapePtr delegate_;
    Axis axis_;
};

} // namespace mc
