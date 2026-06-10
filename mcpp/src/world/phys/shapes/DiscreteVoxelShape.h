#pragma once
#include <cstdint>
#include <functional>
#include <memory>

#include "world/phys/Direction.h"
#include "world/phys/shapes/BooleanOp.h"
#include "world/phys/shapes/JavaMath.h"

// ---------------------------------------------------------------------------
// Port of (Minecraft Java Edition 26.1.2):
//   net/minecraft/world/phys/shapes/DiscreteVoxelShape.java
//   net/minecraft/world/phys/shapes/BitSetDiscreteVoxelShape.java
//   net/minecraft/world/phys/shapes/SubShape.java
//
// NOT ported (hard-absent until its dependency lands, not stubbed):
//   DiscreteVoxelShape.rotate(OctahedralGroup) — needs com/mojang/math/
//   OctahedralGroup, which is unported. No caller in the collision core.
//
// NOTE: the worldgen TU (TreeFeature.h) carries a private partial port of
// forAllFaces for leaf updates; THIS is the canonical public one. The worldgen
// copy gets reconciled onto this later — do not fold it in from here.
// ---------------------------------------------------------------------------

namespace mc {

class IndexMerger;

class DiscreteVoxelShape {
public:
    // Java: protected final int xSize/ySize/zSize (DiscreteVoxelShape.java:10-12).
    const int32_t xSize;
    const int32_t ySize;
    const int32_t zSize;

    virtual ~DiscreteVoxelShape() = default;

    // Java: DiscreteVoxelShape.java:62-64.
    bool isFullWide(AxisCycle transform, int32_t x, int32_t y, int32_t z) const {
        return isFullWide(axisCycleChoose(transform, x, y, z, Axis::X),
                          axisCycleChoose(transform, x, y, z, Axis::Y),
                          axisCycleChoose(transform, x, y, z, Axis::Z));
    }

    // Java: DiscreteVoxelShape.java:66-72.
    bool isFullWide(int32_t x, int32_t y, int32_t z) const {
        if (x < 0 || y < 0 || z < 0) return false;
        return x < xSize && y < ySize && z < zSize ? isFull(x, y, z) : false;
    }

    // Java: DiscreteVoxelShape.java:74-76.
    bool isFull(AxisCycle transform, int32_t x, int32_t y, int32_t z) const {
        return isFull(axisCycleChoose(transform, x, y, z, Axis::X),
                      axisCycleChoose(transform, x, y, z, Axis::Y),
                      axisCycleChoose(transform, x, y, z, Axis::Z));
    }

    virtual bool isFull(int32_t x, int32_t y, int32_t z) const = 0;
    virtual void fill(int32_t x, int32_t y, int32_t z) = 0;

    // Java: DiscreteVoxelShape.java:82-90 (overridden by BitSetDiscreteVoxelShape).
    virtual bool isEmpty() const {
        for (Axis axis : AXIS_VALUES)
            if (firstFull(axis) >= lastFull(axis)) return true;
        return false;
    }

    virtual int32_t firstFull(Axis axis) const = 0;
    virtual int32_t lastFull(Axis axis) const = 0;

    // Java: DiscreteVoxelShape.java:96-117.
    int32_t firstFull(Axis aAxis, int32_t b, int32_t c) const;
    // Java: DiscreteVoxelShape.java:119-140.
    int32_t lastFull(Axis aAxis, int32_t b, int32_t c) const;

    // Java: DiscreteVoxelShape.java:142-144.
    int32_t getSize(Axis axis) const { return axisChoose(axis, xSize, ySize, zSize); }
    int32_t getXSize() const { return getSize(Axis::X); } // :146-148
    int32_t getYSize() const { return getSize(Axis::Y); } // :150-152
    int32_t getZSize() const { return getSize(Axis::Z); } // :154-156

    // Java: DiscreteVoxelShape.IntLineConsumer / IntFaceConsumer (:264-270).
    using IntLineConsumer =
        std::function<void(int32_t, int32_t, int32_t, int32_t, int32_t, int32_t)>;
    using IntFaceConsumer = std::function<void(Direction, int32_t, int32_t, int32_t)>;

    // Java: DiscreteVoxelShape.java:158-162.
    void forAllEdges(const IntLineConsumer& consumer, bool mergeNeighbors) const;
    // Java: DiscreteVoxelShape.java:218-220 (delegates to BitSetDiscreteVoxelShape).
    void forAllBoxes(const IntLineConsumer& consumer, bool mergeNeighbors) const;
    // Java: DiscreteVoxelShape.java:222-226.
    void forAllFaces(const IntFaceConsumer& consumer) const;

protected:
    // Java: DiscreteVoxelShape.java:14-22 (the negative-size IllegalArgumentException
    // is an invariant violated by no caller; sizes here come from the same code paths).
    DiscreteVoxelShape(int32_t xSize, int32_t ySize, int32_t zSize)
        : xSize(xSize), ySize(ySize), zSize(zSize) {}

private:
    void forAllAxisEdges(const IntLineConsumer& consumer, AxisCycle transform,
                         bool mergeNeighbors) const; // :164-216
    void forAllAxisFaces(const IntFaceConsumer& consumer, AxisCycle transform) const; // :228-262
};

using DiscreteVoxelShapePtr = std::shared_ptr<DiscreteVoxelShape>;

// Java: BitSetDiscreteVoxelShape.java.
class BitSetDiscreteVoxelShape final : public DiscreteVoxelShape {
public:
    // Java: BitSetDiscreteVoxelShape.java:15-21.
    BitSetDiscreteVoxelShape(int32_t xSize, int32_t ySize, int32_t zSize)
        : DiscreteVoxelShape(xSize, ySize, zSize),
          storage_(xSize * ySize * zSize),
          xMin_(xSize), yMin_(ySize), zMin_(zSize),
          xMax_(0), yMax_(0), zMax_(0) {}

    // Java: BitSetDiscreteVoxelShape.java:45-69 (copy from any DiscreteVoxelShape).
    explicit BitSetDiscreteVoxelShape(const DiscreteVoxelShape& other);

    // Java: BitSetDiscreteVoxelShape.java:23-43.
    static std::shared_ptr<BitSetDiscreteVoxelShape> withFilledBounds(
        int32_t xSize, int32_t ySize, int32_t zSize,
        int32_t xMin, int32_t yMin, int32_t zMin,
        int32_t xMax, int32_t yMax, int32_t zMax);

    // Java: BitSetDiscreteVoxelShape.java:71-73.
    int32_t getIndex(int32_t x, int32_t y, int32_t z) const {
        return (x * ySize + y) * zSize + z;
    }

    using DiscreteVoxelShape::isFull; // keep the (AxisCycle, ...) overload visible
    bool isFull(int32_t x, int32_t y, int32_t z) const override { // :75-78
        return storage_.get(getIndex(x, y, z));
    }

    void fill(int32_t x, int32_t y, int32_t z) override { // :92-95
        fillUpdateBounds(x, y, z, true);
    }

    bool isEmpty() const override { return storage_.isEmpty(); } // :97-100

    int32_t firstFull(Axis axis) const override { // :102-105
        return axisChoose(axis, xMin_, yMin_, zMin_);
    }
    int32_t lastFull(Axis axis) const override { // :107-110
        return axisChoose(axis, xMax_, yMax_, zMax_);
    }
    using DiscreteVoxelShape::firstFull; // keep the (Axis, b, c) overloads visible
    using DiscreteVoxelShape::lastFull;

    // Java: BitSetDiscreteVoxelShape.join — :112-158.
    static std::shared_ptr<BitSetDiscreteVoxelShape> join(
        const DiscreteVoxelShape& first, const DiscreteVoxelShape& second,
        const IndexMerger& xMerger, const IndexMerger& yMerger, const IndexMerger& zMerger,
        BooleanOp op);

    // Java: BitSetDiscreteVoxelShape.forAllBoxes — :160-200 (the greedy box merger).
    static void forAllBoxesImpl(const DiscreteVoxelShape& voxelShape,
                                const IntLineConsumer& consumer, bool mergeNeighbors);

    // Java: BitSetDiscreteVoxelShape.java:220-230.
    bool isInterior(int32_t x, int32_t y, int32_t z) const;

private:
    void fillUpdateBounds(int32_t x, int32_t y, int32_t z, bool updateBounds) { // :80-90
        storage_.set(getIndex(x, y, z));
        if (updateBounds) {
            xMin_ = xMin_ < x ? xMin_ : x;
            yMin_ = yMin_ < y ? yMin_ : y;
            zMin_ = zMin_ < z ? zMin_ : z;
            xMax_ = xMax_ > x + 1 ? xMax_ : x + 1;
            yMax_ = yMax_ > y + 1 ? yMax_ : y + 1;
            zMax_ = zMax_ > z + 1 ? zMax_ : z + 1;
        }
    }

    bool isZStripFull(int32_t startZ, int32_t endZ, int32_t x, int32_t y) const { // :202-204
        return x < xSize && y < ySize
                   ? storage_.nextClearBit(getIndex(x, y, startZ)) >= getIndex(x, y, endZ)
                   : false;
    }

    bool isXZRectangleFull(int32_t startX, int32_t endX, int32_t startZ, int32_t endZ,
                           int32_t y) const { // :206-214
        for (int32_t x = startX; x < endX; ++x)
            if (!isZStripFull(startZ, endZ, x, y)) return false;
        return true;
    }

    void clearZStrip(int32_t startZ, int32_t endZ, int32_t x, int32_t y) { // :216-218
        storage_.clear(getIndex(x, y, startZ), getIndex(x, y, endZ));
    }

    JavaBitSet storage_;
    int32_t xMin_, yMin_, zMin_;
    int32_t xMax_, yMax_, zMax_;
};

// Java: SubShape.java — a view of a [start, end) sub-box of a parent shape.
// (C++ holds the parent via shared_ptr; Java holds a reference.)
class SubShape final : public DiscreteVoxelShape {
public:
    // Java: SubShape.java:15-24.
    SubShape(DiscreteVoxelShapePtr parent, int32_t startX, int32_t startY, int32_t startZ,
             int32_t endX, int32_t endY, int32_t endZ)
        : DiscreteVoxelShape(endX - startX, endY - startY, endZ - startZ),
          parent_(std::move(parent)),
          startX_(startX), startY_(startY), startZ_(startZ),
          endX_(endX), endY_(endY), endZ_(endZ) {}

    using DiscreteVoxelShape::isFull;
    bool isFull(int32_t x, int32_t y, int32_t z) const override { // :26-29
        return parent_->isFull(startX_ + x, startY_ + y, startZ_ + z);
    }

    void fill(int32_t x, int32_t y, int32_t z) override { // :31-34
        parent_->fill(startX_ + x, startY_ + y, startZ_ + z);
    }

    int32_t firstFull(Axis axis) const override { // :36-39
        return clampToShape(axis, parent_->firstFull(axis));
    }
    int32_t lastFull(Axis axis) const override { // :41-44
        return clampToShape(axis, parent_->lastFull(axis));
    }
    using DiscreteVoxelShape::firstFull;
    using DiscreteVoxelShape::lastFull;

private:
    int32_t clampToShape(Axis axis, int32_t parentResult) const { // :46-50
        int32_t start = axisChoose(axis, startX_, startY_, startZ_);
        int32_t end = axisChoose(axis, endX_, endY_, endZ_);
        return mthClamp(parentResult, start, end) - start;
    }

    DiscreteVoxelShapePtr parent_;
    int32_t startX_, startY_, startZ_;
    int32_t endX_, endY_, endZ_;
};

} // namespace mc
