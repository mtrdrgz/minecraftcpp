#include "NetherFossilStructure.h"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
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

bool runSelfCheck() {
    using namespace mc::levelgen::structure;
    using namespace mc::levelgen::structure::structures;

    const NetherFossilColumnView successColumn{
        .isAir = [](int, int y, int) { return y == 90 || y == 13; },
        .isSoulSand = [](int, int y, int) { return y == 89; },
        .isFaceSturdyUp = [](int, int y, int) { return y == 12; },
    };

    {
        // Constant height: x/z are drawn first, then the fossil piece consumes rotation+template.
        ScriptedRandom random({16, 16, 4, 14}, {5, 9, 2, 13});
        const NetherFossilGenerationConfig cfg{NetherFossilHeightProvider::constant(90), 32};
        auto got = findNetherFossilGenerationPoint(2, -3, cfg, successColumn, random);
        assert(got.has_value());
        assert(got->position == (BlockPos{37, 89, -39}));
        assert(got->piece.rotation == Rotation::CLOCKWISE_180);
        assert(std::string(got->piece.templateId) == "minecraft:nether_fossils/fossil_14");
        assert(random.calls() == 4);
    }

    {
        // Uniform height samples AFTER x/z and BEFORE column scan / piece selection.
        ScriptedRandom random({16, 16, 7, 4, 14}, {1, 2, 3, 0, 0});
        const NetherFossilGenerationConfig cfg{NetherFossilHeightProvider::uniform(10, 16), 5};
        auto got = findNetherFossilGenerationPoint(0, 0, cfg, successColumn, random);
        assert(got.has_value());
        assert(got->position == (BlockPos{1, 12, 2}));
        assert(got->piece.rotation == Rotation::NONE);
        assert(std::string(got->piece.templateId) == "minecraft:nether_fossils/fossil_1");
        assert(random.calls() == 5);
    }

    {
        // If the downward scan reaches sea level, Java returns Optional.empty and addPieces is not called.
        const NetherFossilColumnView missColumn{
            .isAir = [](int, int, int) { return false; },
            .isSoulSand = [](int, int, int) { return false; },
            .isFaceSturdyUp = [](int, int, int) { return false; },
        };
        ScriptedRandom random({16, 16}, {4, 4});
        const NetherFossilGenerationConfig cfg{NetherFossilHeightProvider::constant(34), 32};
        auto got = findNetherFossilGenerationPoint(0, 0, cfg, missColumn, random);
        assert(!got.has_value());
        assert(random.calls() == 2);
    }

    return true;
}

} // namespace

int main() {
    runSelfCheck();
    std::cout << "NetherFossilStructure generation-point slice OK\n";
    return 0;
}
