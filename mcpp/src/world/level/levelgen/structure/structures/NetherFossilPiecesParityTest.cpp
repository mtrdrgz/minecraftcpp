#include "NetherFossilPieces.h"

#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(line);
    while (std::getline(in, cur, '\t')) out.push_back(cur);
    return out;
}

mc::levelgen::structure::Rotation parseRotation(const std::string& name) {
    using mc::levelgen::structure::Rotation;
    if (name == "NONE") return Rotation::NONE;
    if (name == "CLOCKWISE_90") return Rotation::CLOCKWISE_90;
    if (name == "CLOCKWISE_180") return Rotation::CLOCKWISE_180;
    if (name == "COUNTERCLOCKWISE_90") return Rotation::COUNTERCLOCKWISE_90;
    throw std::runtime_error("unknown rotation: " + name);
}

bool runSelfCheck() {
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

    return true;
}

bool verifyCaseLine(const std::string& line, std::string& err) {
    using namespace mc::levelgen::structure::structures;
    if (line.empty()) return true;
    const std::vector<std::string> parts = splitTabs(line);
    if (parts.empty() || parts[0].empty()) return true;
    if (parts[0] != "CASE") return true;
    if (parts.size() != 4) {
        err = "bad CASE column count";
        return false;
    }

    const int64_t seed = std::stoll(parts[1]);
    const auto expectedRotation = parseRotation(parts[2]);
    const std::string& expectedTemplate = parts[3];

    std::shared_ptr<mc::levelgen::RandomSource> random = mc::levelgen::RandomSource::create(seed);
    const NetherFossilPieceSelection got = selectNetherFossilPiece(*random);
    if (got.rotation != expectedRotation || expectedTemplate != got.templateId) {
        err = "seed=" + parts[1] + " got=" + std::string(got.templateId) + "/" + std::to_string(static_cast<int>(got.rotation)) +
              " expected=" + expectedTemplate + "/" + parts[2];
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) {
            std::cerr << "cannot open " << argv[2] << '\n';
            return 2;
        }
        std::string line;
        long cases = 0;
        long bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            if (!verifyCaseLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << " | " << line << '\n';
            }
            if (line.rfind("CASE\t", 0) == 0) ++cases;
        }
        std::cout << "NetherFossilPieces cases=" << cases << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    runSelfCheck();
    std::cout << "NetherFossilPieces deterministic selection OK\n";
    return 0;
}
