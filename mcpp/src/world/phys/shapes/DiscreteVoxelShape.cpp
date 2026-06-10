#include "world/phys/shapes/DiscreteVoxelShape.h"

#include <climits>
#include <limits>

#include "world/phys/shapes/IndexMerger.h"

// Port of DiscreteVoxelShape.java + BitSetDiscreteVoxelShape.java method bodies
// (Minecraft Java Edition 26.1.2). Every block cites the Java lines it mirrors.

namespace mc {

// ---------------------------------------------------------------------------
// DiscreteVoxelShape
// ---------------------------------------------------------------------------

// Java: DiscreteVoxelShape.java:96-117.
int32_t DiscreteVoxelShape::firstFull(Axis aAxis, int32_t b, int32_t c) const {
    int32_t aSize = getSize(aAxis);
    if (b >= 0 && c >= 0) {
        Axis bAxis = axisCycleAxis(AxisCycle::FORWARD, aAxis);
        Axis cAxis = axisCycleAxis(AxisCycle::BACKWARD, aAxis);
        if (b < getSize(bAxis) && c < getSize(cAxis)) {
            AxisCycle transform = axisCycleBetween(Axis::X, aAxis);
            for (int32_t a = 0; a < aSize; ++a)
                if (isFull(transform, a, b, c)) return a;
            return aSize;
        }
        return aSize;
    }
    return aSize;
}

// Java: DiscreteVoxelShape.java:119-140.
int32_t DiscreteVoxelShape::lastFull(Axis aAxis, int32_t b, int32_t c) const {
    if (b >= 0 && c >= 0) {
        Axis bAxis = axisCycleAxis(AxisCycle::FORWARD, aAxis);
        Axis cAxis = axisCycleAxis(AxisCycle::BACKWARD, aAxis);
        if (b < getSize(bAxis) && c < getSize(cAxis)) {
            int32_t aSize = getSize(aAxis);
            AxisCycle transform = axisCycleBetween(Axis::X, aAxis);
            for (int32_t a = aSize - 1; a >= 0; --a)
                if (isFull(transform, a, b, c)) return a + 1;
            return 0;
        }
        return 0;
    }
    return 0;
}

// Java: DiscreteVoxelShape.java:158-162.
void DiscreteVoxelShape::forAllEdges(const IntLineConsumer& consumer, bool mergeNeighbors) const {
    forAllAxisEdges(consumer, AxisCycle::NONE, mergeNeighbors);
    forAllAxisEdges(consumer, AxisCycle::FORWARD, mergeNeighbors);
    forAllAxisEdges(consumer, AxisCycle::BACKWARD, mergeNeighbors);
}

// Java: DiscreteVoxelShape.java:164-216.
void DiscreteVoxelShape::forAllAxisEdges(const IntLineConsumer& consumer, AxisCycle transform,
                                         bool mergeNeighbors) const {
    AxisCycle inverse = axisCycleInverse(transform);
    int32_t aSize = getSize(axisCycleAxis(inverse, Axis::X));
    int32_t bSize = getSize(axisCycleAxis(inverse, Axis::Y));
    int32_t cSize = getSize(axisCycleAxis(inverse, Axis::Z));

    for (int32_t a = 0; a <= aSize; ++a) {
        for (int32_t b = 0; b <= bSize; ++b) {
            int32_t lastStart = -1;
            for (int32_t c = 0; c <= cSize; ++c) {
                int32_t fullSectors = 0;
                int32_t oddSectors = 0;
                for (int32_t da = 0; da <= 1; ++da) {
                    for (int32_t db = 0; db <= 1; ++db) {
                        if (isFullWide(inverse, a + da - 1, b + db - 1, c)) {
                            ++fullSectors;
                            oddSectors ^= da ^ db;
                        }
                    }
                }

                if (fullSectors == 1 || fullSectors == 3
                    || (fullSectors == 2 && (oddSectors & 1) == 0)) {
                    if (mergeNeighbors) {
                        if (lastStart == -1) lastStart = c;
                    } else {
                        consumer(axisCycleChoose(inverse, a, b, c, Axis::X),
                                 axisCycleChoose(inverse, a, b, c, Axis::Y),
                                 axisCycleChoose(inverse, a, b, c, Axis::Z),
                                 axisCycleChoose(inverse, a, b, c + 1, Axis::X),
                                 axisCycleChoose(inverse, a, b, c + 1, Axis::Y),
                                 axisCycleChoose(inverse, a, b, c + 1, Axis::Z));
                    }
                } else if (lastStart != -1) {
                    consumer(axisCycleChoose(inverse, a, b, lastStart, Axis::X),
                             axisCycleChoose(inverse, a, b, lastStart, Axis::Y),
                             axisCycleChoose(inverse, a, b, lastStart, Axis::Z),
                             axisCycleChoose(inverse, a, b, c, Axis::X),
                             axisCycleChoose(inverse, a, b, c, Axis::Y),
                             axisCycleChoose(inverse, a, b, c, Axis::Z));
                    lastStart = -1;
                }
            }
        }
    }
}

// Java: DiscreteVoxelShape.java:218-220.
void DiscreteVoxelShape::forAllBoxes(const IntLineConsumer& consumer, bool mergeNeighbors) const {
    BitSetDiscreteVoxelShape::forAllBoxesImpl(*this, consumer, mergeNeighbors);
}

// Java: DiscreteVoxelShape.java:222-226.
void DiscreteVoxelShape::forAllFaces(const IntFaceConsumer& consumer) const {
    forAllAxisFaces(consumer, AxisCycle::NONE);
    forAllAxisFaces(consumer, AxisCycle::FORWARD);
    forAllAxisFaces(consumer, AxisCycle::BACKWARD);
}

// Java: DiscreteVoxelShape.java:228-262.
void DiscreteVoxelShape::forAllAxisFaces(const IntFaceConsumer& consumer,
                                         AxisCycle transform) const {
    AxisCycle inverse = axisCycleInverse(transform);
    Axis cAxis = axisCycleAxis(inverse, Axis::Z);
    int32_t aSize = getSize(axisCycleAxis(inverse, Axis::X));
    int32_t bSize = getSize(axisCycleAxis(inverse, Axis::Y));
    int32_t cSize = getSize(cAxis);
    Direction negative = directionFromAxisAndDirection(cAxis, AxisDirection::NEGATIVE);
    Direction positive = directionFromAxisAndDirection(cAxis, AxisDirection::POSITIVE);

    for (int32_t a = 0; a < aSize; ++a) {
        for (int32_t b = 0; b < bSize; ++b) {
            bool lastFull = false;
            for (int32_t c = 0; c <= cSize; ++c) {
                bool full = c != cSize && isFull(inverse, a, b, c);
                if (!lastFull && full) {
                    consumer(negative,
                             axisCycleChoose(inverse, a, b, c, Axis::X),
                             axisCycleChoose(inverse, a, b, c, Axis::Y),
                             axisCycleChoose(inverse, a, b, c, Axis::Z));
                }
                if (lastFull && !full) {
                    consumer(positive,
                             axisCycleChoose(inverse, a, b, c - 1, Axis::X),
                             axisCycleChoose(inverse, a, b, c - 1, Axis::Y),
                             axisCycleChoose(inverse, a, b, c - 1, Axis::Z));
                }
                lastFull = full;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// BitSetDiscreteVoxelShape
// ---------------------------------------------------------------------------

// Java: BitSetDiscreteVoxelShape.java:45-69.
BitSetDiscreteVoxelShape::BitSetDiscreteVoxelShape(const DiscreteVoxelShape& other)
    : DiscreteVoxelShape(other.xSize, other.ySize, other.zSize),
      storage_(other.xSize * other.ySize * other.zSize),
      xMin_(0), yMin_(0), zMin_(0), xMax_(0), yMax_(0), zMax_(0) {
    // (Java clones the BitSet when other is a BitSetDiscreteVoxelShape — a pure
    // optimization; the per-voxel copy below produces the identical BitSet.)
    for (int32_t x = 0; x < xSize; ++x)
        for (int32_t y = 0; y < ySize; ++y)
            for (int32_t z = 0; z < zSize; ++z)
                if (other.isFull(x, y, z)) storage_.set(getIndex(x, y, z));

    xMin_ = other.firstFull(Axis::X);
    yMin_ = other.firstFull(Axis::Y);
    zMin_ = other.firstFull(Axis::Z);
    xMax_ = other.lastFull(Axis::X);
    yMax_ = other.lastFull(Axis::Y);
    zMax_ = other.lastFull(Axis::Z);
}

// Java: BitSetDiscreteVoxelShape.java:23-43.
std::shared_ptr<BitSetDiscreteVoxelShape> BitSetDiscreteVoxelShape::withFilledBounds(
    int32_t xSize, int32_t ySize, int32_t zSize,
    int32_t xMin, int32_t yMin, int32_t zMin,
    int32_t xMax, int32_t yMax, int32_t zMax) {
    auto shape = std::make_shared<BitSetDiscreteVoxelShape>(xSize, ySize, zSize);
    shape->xMin_ = xMin;
    shape->yMin_ = yMin;
    shape->zMin_ = zMin;
    shape->xMax_ = xMax;
    shape->yMax_ = yMax;
    shape->zMax_ = zMax;

    for (int32_t x = xMin; x < xMax; ++x)
        for (int32_t y = yMin; y < yMax; ++y)
            for (int32_t z = zMin; z < zMax; ++z)
                shape->fillUpdateBounds(x, y, z, false);

    return shape;
}

// Java: BitSetDiscreteVoxelShape.join — :112-158.
std::shared_ptr<BitSetDiscreteVoxelShape> BitSetDiscreteVoxelShape::join(
    const DiscreteVoxelShape& first, const DiscreteVoxelShape& second,
    const IndexMerger& xMerger, const IndexMerger& yMerger, const IndexMerger& zMerger,
    BooleanOp op) {
    auto shape = std::make_shared<BitSetDiscreteVoxelShape>(
        xMerger.size() - 1, yMerger.size() - 1, zMerger.size() - 1);
    int32_t bounds[6] = {INT32_MAX, INT32_MAX, INT32_MAX, INT32_MIN, INT32_MIN, INT32_MIN};
    xMerger.forMergedIndexes([&](int32_t x1, int32_t x2, int32_t xr) {
        bool updatedSlice = false;
        yMerger.forMergedIndexes([&](int32_t y1, int32_t y2, int32_t yr) {
            bool updatedColumn = false;
            zMerger.forMergedIndexes([&](int32_t z1, int32_t z2, int32_t zr) {
                if (op.apply(first.isFullWide(x1, y1, z1), second.isFullWide(x2, y2, z2))) {
                    shape->storage_.set(shape->getIndex(xr, yr, zr));
                    bounds[2] = bounds[2] < zr ? bounds[2] : zr;
                    bounds[5] = bounds[5] > zr ? bounds[5] : zr;
                    updatedColumn = true;
                }
                return true;
            });
            if (updatedColumn) {
                bounds[1] = bounds[1] < yr ? bounds[1] : yr;
                bounds[4] = bounds[4] > yr ? bounds[4] : yr;
                updatedSlice = true;
            }
            return true;
        });
        if (updatedSlice) {
            bounds[0] = bounds[0] < xr ? bounds[0] : xr;
            bounds[3] = bounds[3] > xr ? bounds[3] : xr;
        }
        return true;
    });
    shape->xMin_ = bounds[0];
    shape->yMin_ = bounds[1];
    shape->zMin_ = bounds[2];
    shape->xMax_ = bounds[3] + 1;
    shape->yMax_ = bounds[4] + 1;
    shape->zMax_ = bounds[5] + 1;
    return shape;
}

// Java: BitSetDiscreteVoxelShape.forAllBoxes — :160-200. Greedy merge of full
// voxels into maximal boxes (operates on a consumable copy).
void BitSetDiscreteVoxelShape::forAllBoxesImpl(const DiscreteVoxelShape& voxelShape,
                                               const IntLineConsumer& consumer,
                                               bool mergeNeighbors) {
    BitSetDiscreteVoxelShape shape(voxelShape);

    for (int32_t y = 0; y < shape.ySize; ++y) {
        for (int32_t x = 0; x < shape.xSize; ++x) {
            int32_t lastStartZ = -1;
            for (int32_t z = 0; z <= shape.zSize; ++z) {
                if (shape.isFullWide(x, y, z)) {
                    if (mergeNeighbors) {
                        if (lastStartZ == -1) lastStartZ = z;
                    } else {
                        consumer(x, y, z, x + 1, y + 1, z + 1);
                    }
                } else if (lastStartZ != -1) {
                    int32_t endX = x;
                    int32_t endY = y;
                    shape.clearZStrip(lastStartZ, z, x, y);

                    while (shape.isZStripFull(lastStartZ, z, endX + 1, y)) {
                        shape.clearZStrip(lastStartZ, z, endX + 1, y);
                        ++endX;
                    }

                    while (shape.isXZRectangleFull(x, endX + 1, lastStartZ, z, endY + 1)) {
                        for (int32_t cx = x; cx <= endX; ++cx)
                            shape.clearZStrip(lastStartZ, z, cx, endY + 1);
                        ++endY;
                    }

                    consumer(x, y, lastStartZ, endX + 1, endY + 1, z);
                    lastStartZ = -1;
                }
            }
        }
    }
}

// Java: BitSetDiscreteVoxelShape.java:220-230.
bool BitSetDiscreteVoxelShape::isInterior(int32_t x, int32_t y, int32_t z) const {
    bool interior = x > 0 && x < xSize - 1 && y > 0 && y < ySize - 1 && z > 0 && z < zSize - 1;
    return interior && isFull(x, y, z)
        && isFull(x - 1, y, z) && isFull(x + 1, y, z)
        && isFull(x, y - 1, z) && isFull(x, y + 1, z)
        && isFull(x, y, z - 1) && isFull(x, y, z + 1);
}

// ---------------------------------------------------------------------------
// IndirectMerger ctor — Java: IndirectMerger.java:14-63 (verbatim, including
// the NaN seed for lastValue: `!(NaN >= x)` is true, so the first accepted
// point always writes a fresh entry).
// ---------------------------------------------------------------------------
IndirectMerger::IndirectMerger(const DoubleList& first, const DoubleList& second,
                               bool firstOnlyMatters, bool secondOnlyMatters) {
    double lastValue = std::numeric_limits<double>::quiet_NaN();
    int32_t firstSize = first.size();
    int32_t secondSize = second.size();
    int32_t capacity = firstSize + secondSize;
    result_.assign(static_cast<size_t>(capacity), 0.0);
    firstIndices_.assign(static_cast<size_t>(capacity), 0);
    secondIndices_.assign(static_cast<size_t>(capacity), 0);
    bool canSkipFirst = !firstOnlyMatters;
    bool canSkipSecond = !secondOnlyMatters;
    int32_t resultIndex = 0;
    int32_t firstIndex = 0;
    int32_t secondIndex = 0;

    while (true) {
        bool ranOutOfFirst = firstIndex >= firstSize;
        bool ranOutOfSecond = secondIndex >= secondSize;
        if (ranOutOfFirst && ranOutOfSecond) {
            resultLength_ = resultIndex > 1 ? resultIndex : 1; // Math.max(1, resultIndex)
            return;
        }

        bool choseFirst = !ranOutOfFirst
            && (ranOutOfSecond
                || first.getDouble(firstIndex) < second.getDouble(secondIndex) + 1.0E-7);
        if (choseFirst) {
            ++firstIndex;
            if (canSkipFirst && (secondIndex == 0 || ranOutOfSecond)) continue;
        } else {
            ++secondIndex;
            if (canSkipSecond && (firstIndex == 0 || ranOutOfFirst)) continue;
        }

        int32_t currentFirstIndex = firstIndex - 1;
        int32_t currentSecondIndex = secondIndex - 1;
        double nextValue =
            choseFirst ? first.getDouble(currentFirstIndex) : second.getDouble(currentSecondIndex);
        if (!(lastValue >= nextValue - 1.0E-7)) {
            firstIndices_[resultIndex] = currentFirstIndex;
            secondIndices_[resultIndex] = currentSecondIndex;
            result_[resultIndex] = nextValue;
            ++resultIndex;
            lastValue = nextValue;
        } else {
            firstIndices_[resultIndex - 1] = currentFirstIndex;
            secondIndices_[resultIndex - 1] = currentSecondIndex;
        }
    }
}

} // namespace mc
