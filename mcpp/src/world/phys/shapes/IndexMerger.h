#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "world/phys/shapes/DoubleList.h"
#include "world/phys/shapes/JavaMath.h"

// ---------------------------------------------------------------------------
// Port of the IndexMerger family (Minecraft Java Edition 26.1.2):
//   net/minecraft/world/phys/shapes/IndexMerger.java
//   net/minecraft/world/phys/shapes/IdenticalMerger.java
//   net/minecraft/world/phys/shapes/DiscreteCubeMerger.java
//   net/minecraft/world/phys/shapes/NonOverlappingMerger.java
//   net/minecraft/world/phys/shapes/IndirectMerger.java
// ---------------------------------------------------------------------------

namespace mc {

class IndexMerger {
public:
    virtual ~IndexMerger() = default;
    // Java: IndexMerger.IndexConsumer.merge(firstIndex, secondIndex, resultIndex)
    // returning false aborts the iteration (IndexMerger.java:12-14).
    using IndexConsumer = std::function<bool(int32_t, int32_t, int32_t)>;

    virtual DoubleListPtr getList() const = 0;
    virtual bool forMergedIndexes(const IndexConsumer& consumer) const = 0;
    virtual int32_t size() const = 0;
    // Java: `instanceof DiscreteCubeMerger` (Shapes.java:169) — RTTI-free.
    virtual bool isDiscreteCubeMerger() const noexcept { return false; }
};

using IndexMergerPtr = std::shared_ptr<const IndexMerger>;

// Java: IdenticalMerger.java — both shapes share one coordinate list.
class IdenticalMerger final : public IndexMerger {
public:
    explicit IdenticalMerger(DoubleListPtr coords) : coords_(std::move(coords)) {}

    bool forMergedIndexes(const IndexConsumer& consumer) const override { // :12-23
        int32_t size = coords_->size() - 1;
        for (int32_t i = 0; i < size; ++i)
            if (!consumer(i, i, i)) return false;
        return true;
    }

    int32_t size() const override { return coords_->size(); } // :25-28
    DoubleListPtr getList() const override { return coords_; } // :30-33

private:
    DoubleListPtr coords_;
};

// Java: DiscreteCubeMerger.java — both lists are CubePointRanges; merge on the lcm grid.
class DiscreteCubeMerger final : public IndexMerger {
public:
    // Java: DiscreteCubeMerger.java:11-16. Shapes.lcm(a, b) = (long)a * (b / gcd(a, b))
    // (Shapes.java:122-124); the ctor casts it to int (guarded <= 256 by the caller).
    DiscreteCubeMerger(int32_t firstSize, int32_t secondSize)
        : result_(std::make_shared<CubePointRange>(static_cast<int32_t>(
              static_cast<int64_t>(firstSize) * (secondSize / intMathGcd(firstSize, secondSize))))) {
        const int32_t gcd = intMathGcd(firstSize, secondSize);
        firstDiv_ = firstSize / gcd;
        secondDiv_ = secondSize / gcd;
    }

    bool forMergedIndexes(const IndexConsumer& consumer) const override { // :18-29
        int32_t size = result_->size() - 1;
        for (int32_t i = 0; i < size; ++i)
            if (!consumer(i / secondDiv_, i / firstDiv_, i)) return false;
        return true;
    }

    int32_t size() const override { return result_->size(); } // :31-34
    DoubleListPtr getList() const override { return result_; } // :36-39
    bool isDiscreteCubeMerger() const noexcept override { return true; }

private:
    std::shared_ptr<const CubePointRange> result_;
    int32_t firstDiv_;
    int32_t secondDiv_;
};

// Java: NonOverlappingMerger.java — extends AbstractDoubleList AND implements
// IndexMerger (it IS its own merged coordinate list); same dual role here.
class NonOverlappingMerger final : public DoubleList,
                                   public IndexMerger,
                                   public std::enable_shared_from_this<NonOverlappingMerger> {
public:
    NonOverlappingMerger(DoubleListPtr lower, DoubleListPtr upper, bool swap) // :11-15
        : lower_(std::move(lower)), upper_(std::move(upper)), swap_(swap) {}

    int32_t size() const override { return lower_->size() + upper_->size(); } // :17-20

    bool forMergedIndexes(const IndexConsumer& consumer) const override { // :22-27
        return swap_ ? forNonSwappedIndexes([&consumer](int32_t f, int32_t s, int32_t r) {
                          return consumer(s, f, r);
                      })
                     : forNonSwappedIndexes(consumer);
    }

    double getDouble(int32_t index) const override { // :49-51
        return index < lower_->size() ? lower_->getDouble(index)
                                      : upper_->getDouble(index - lower_->size());
    }

    DoubleListPtr getList() const override { return shared_from_this(); } // :53-56

private:
    bool forNonSwappedIndexes(const IndexConsumer& consumer) const { // :29-47
        int32_t lowerSize = lower_->size();
        for (int32_t i = 0; i < lowerSize; ++i)
            if (!consumer(i, -1, i)) return false;
        int32_t upperSize = upper_->size() - 1;
        for (int32_t i = 0; i < upperSize; ++i)
            if (!consumer(lowerSize - 1, i, lowerSize + i)) return false;
        return true;
    }

    DoubleListPtr lower_;
    DoubleListPtr upper_;
    bool swap_;
};

// Java: IndirectMerger.java — the general fuzzy merge of two coordinate lists.
class IndirectMerger final : public IndexMerger {
public:
    IndirectMerger(const DoubleList& first, const DoubleList& second,
                   bool firstOnlyMatters, bool secondOnlyMatters); // :14-63

    bool forMergedIndexes(const IndexConsumer& consumer) const override { // :65-76
        int32_t length = resultLength_ - 1;
        for (int32_t i = 0; i < length; ++i)
            if (!consumer(firstIndices_[i], secondIndices_[i], i)) return false;
        return true;
    }

    int32_t size() const override { return resultLength_; } // :78-81

    DoubleListPtr getList() const override { // :83-86
        if (resultLength_ <= 1) {
            // Java: static EMPTY = unmodifiable wrap(new double[]{0.0}).
            static const DoubleListPtr EMPTY =
                std::make_shared<DoubleArrayList>(std::vector<double>{0.0});
            return EMPTY;
        }
        return std::make_shared<DoubleArrayList>(
            std::vector<double>(result_.begin(), result_.begin() + resultLength_));
    }

private:
    std::vector<double> result_;
    std::vector<int32_t> firstIndices_;
    std::vector<int32_t> secondIndices_;
    int32_t resultLength_;
};

} // namespace mc
