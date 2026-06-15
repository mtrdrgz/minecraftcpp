#include "NetherFossilPieces.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

class ScriptedRandom final : public mc::levelgen::RandomSource {
public:
    ScriptedRandom(std::vector<int32_t> bounds, std::vector<int32_t> values)
        : bounds_(std::move(bounds)), values_(std::move(values)) {}

    std::shared_ptr<mc::levelgen::RandomSource> fork() override { return nullptr; }
    std::shared_ptr<mc::levelgen::PositionalRandomFactory> forkPositional() override { return nullptr; }
    void setSeed(int64_t) override {}
    int32_t nextInt() override { throw std::runtime_error("unexpected unbounded nextInt"); }
    int32_t nextInt(int32_t bound) override {
        if (cursor_ >= bounds_.size() || cursor_ >= values_.size()) {
            throw std::runtime_error("too many nextInt(bound) calls");
        }
        const int32_t expected = bounds_[cursor_];
        const int32_t value = values_[cursor_++];
        if (bound != expected) {
            throw std::runtime_error("unexpected nextInt bound");
        }
        if (value < 0 || value >= bound) {
            throw std::runtime_error("scripted value outside bound");
        }
        return value;
    }
    int64_t nextLong() override { throw std::runtime_error("unexpected nextLong"); }
    bool nextBoolean() override { throw std::runtime_error("unexpected nextBoolean"); }
    float nextFloat() override { throw std::runtime_error("unexpected nextFloat"); }
    double nextDouble() override { throw std::runtime_error("unexpected nextDouble"); }
    double nextGaussian() override { throw std::runtime_error("unexpected nextGaussian"); }

    std::size_t calls() const noexcept { return cursor_; }

private:
    std::vector<int32_t> bounds_;
    std::vector<int32_t> values_;
    std::size_t cursor_ = 0;
};

} // namespace

int main() {
    using namespace mc::levelgen::structure;
    using namespace mc::levelgen::structure::structures;

    static_assert(netherFossilTemplateCount() == 14);
    static_assert(netherFossilRotationByJavaIndex(0) == Rotation::NONE);
    static_assert(netherFossilRotationByJavaIndex(1) == Rotation::CLOCKWISE_90);
    static_assert(netherFossilRotationByJavaIndex(2) == Rotation::CLOCKWISE_180);
    static_assert(netherFossilRotationByJavaIndex(3) == Rotation::COUNTERCLOCKWISE_90);

    assert(std::string(kNetherFossilTemplates.front()) == "minecraft:nether_fossils/fossil_1");
    assert(std::string(kNetherFossilTemplates.back()) == "minecraft:nether_fossils/fossil_14");

    {
        // Java order in NetherFossilPieces.addPieces is rotation first, fossil second.
        ScriptedRandom random({4, 14}, {2, 13});
        const NetherFossilPieceSelection selection = selectNetherFossilPiece(random);
        assert(selection.rotation == Rotation::CLOCKWISE_180);
        assert(std::string(selection.templateId) == "minecraft:nether_fossils/fossil_14");
        assert(random.calls() == 2);
    }

    {
        ScriptedRandom random({4, 14}, {3, 0});
        const NetherFossilPieceSelection selection = selectNetherFossilPiece(random);
        assert(selection.rotation == Rotation::COUNTERCLOCKWISE_90);
        assert(std::string(selection.templateId) == "minecraft:nether_fossils/fossil_1");
        assert(random.calls() == 2);
    }

    std::cout << "NetherFossilPieces deterministic selection OK\n";
    return 0;
}
