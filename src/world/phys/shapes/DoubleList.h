#pragma once
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// The it.unimi.dsi.fastutil DoubleList surface the voxel-shape code uses, plus
// the two Minecraft list implementations (Minecraft Java Edition 26.1.2):
//
//   fastutil DoubleList            — getDouble(int), size(), content equals()
//   fastutil DoubleArrayList       — array-backed list (wrap())
//   net/minecraft/world/phys/shapes/CubePointRange.java   (i / parts grid)
//   net/minecraft/world/phys/shapes/OffsetDoubleList.java (delegate + offset)
//
// Java type checks ported faithfully:
//   * Shapes.createIndexMerger does `first instanceof CubePointRange` —
//     modeled as the virtual isCubePointRange() (RTTI-free).
//   * `Objects.equals(first, second)` on fastutil lists is CONTENT equality
//     (element-wise `!=` over doubles) — doubleListContentEquals below.
// ---------------------------------------------------------------------------

namespace mc {

class DoubleList {
public:
    virtual ~DoubleList() = default;
    virtual double getDouble(int32_t index) const = 0;
    virtual int32_t size() const = 0;
    // Java: `instanceof CubePointRange` (Shapes.java:300).
    virtual bool isCubePointRange() const noexcept { return false; }
};

using DoubleListPtr = std::shared_ptr<const DoubleList>;

// fastutil DoubleArrayList (only the wrap()-style array-backed behavior used here).
class DoubleArrayList final : public DoubleList {
public:
    explicit DoubleArrayList(std::vector<double> values) : values_(std::move(values)) {}
    double getDouble(int32_t index) const override { return values_[static_cast<size_t>(index)]; }
    int32_t size() const override { return static_cast<int32_t>(values_.size()); }

private:
    std::vector<double> values_;
};

// Java: CubePointRange.java — getDouble(i) = (double)i / parts; size() = parts + 1.
class CubePointRange final : public DoubleList {
public:
    explicit CubePointRange(int32_t parts) : parts_(parts) {
        // CubePointRange.java:9-11 throws on parts <= 0; the C++ callers preserve
        // the same invariants, so this is a hard invariant, not a runtime branch.
    }
    double getDouble(int32_t index) const override {
        return static_cast<double>(index) / parts_; // CubePointRange.java:16-18
    }
    int32_t size() const override { return parts_ + 1; } // CubePointRange.java:20-22
    bool isCubePointRange() const noexcept override { return true; }

private:
    int32_t parts_;
};

// Java: OffsetDoubleList.java — delegate.getDouble(i) + offset.
class OffsetDoubleList final : public DoubleList {
public:
    OffsetDoubleList(DoubleListPtr delegate, double offset)
        : delegate_(std::move(delegate)), offset_(offset) {}
    double getDouble(int32_t index) const override {
        return delegate_->getDouble(index) + offset_; // OffsetDoubleList.java:15-17
    }
    int32_t size() const override { return delegate_->size(); } // OffsetDoubleList.java:19-21

private:
    DoubleListPtr delegate_;
    double offset_;
};

// fastutil AbstractDoubleList.equals: same size, element-wise nextDouble() !=
// comparison (so NaN != NaN ⇒ unequal; +0.0 == -0.0 ⇒ equal). Used for the
// IdenticalMerger selection in Shapes.createIndexMerger (Shapes.java:312).
inline bool doubleListContentEquals(const DoubleList& a, const DoubleList& b) {
    const int32_t n = a.size();
    if (n != b.size()) return false;
    for (int32_t i = 0; i < n; ++i)
        if (a.getDouble(i) != b.getDouble(i)) return false;
    return true;
}

} // namespace mc
