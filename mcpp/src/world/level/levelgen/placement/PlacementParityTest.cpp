// Parity test for mc::valueproviders::IntProvider and the pure
// mc::levelgen::placement modifiers. Ground truth: tools/PlacementParity.java
// (the real decompiled value providers + placement modifiers).
//
//   default        -> hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference

#include "../IntProvider.h"
#include "../RandomSource.h"
#include "PlacementModifier.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::valueproviders;
using namespace mc::levelgen::placement;

namespace {

IntProviderPtr wl(std::vector<WeightedListInt::Entry> entries) {
    return std::make_shared<WeightedListInt>(std::move(entries));
}

std::map<std::string, IntProviderPtr> buildIntProviders() {
    return {
        { "const5", ConstantInt::of(5) },
        { "uni1_3", UniformInt::of(1, 3) },
        { "uni0_7", UniformInt::of(0, 7) },
        { "bias0_4", BiasedToBottomInt::of(0, 4) },
        { "clamp_uni", ClampedInt::of(UniformInt::of(-5, 10), 0, 8) },
        { "wl_19_1", wl({ { ConstantInt::of(0), 19 }, { ConstantInt::of(1), 1 } }) },
        { "wl_mixed", wl({ { UniformInt::of(1, 2), 3 }, { ConstantInt::of(5), 1 } }) },
    };
}

std::map<std::string, std::shared_ptr<const PlacementModifier>> buildModifiers() {
    std::map<std::string, std::shared_ptr<const PlacementModifier>> m;
    m["insquare"] = std::make_shared<InSquarePlacement>();
    m["count64"] = std::make_shared<CountPlacement>(ConstantInt::of(64));
    m["count_uni"] = std::make_shared<CountPlacement>(UniformInt::of(2, 5));
    m["count_wl"] = std::make_shared<CountPlacement>(wl({ { ConstantInt::of(0), 19 }, { ConstantInt::of(1), 1 } }));
    m["rarity7"] = std::make_shared<RarityFilter>(7);
    m["rarity32"] = std::make_shared<RarityFilter>(32);
    m["roff_v"] = std::make_shared<RandomOffsetPlacement>(ConstantInt::of(0), UniformInt::of(-2, 2));
    m["roff_h"] = std::make_shared<RandomOffsetPlacement>(UniformInt::of(-3, 3), ConstantInt::of(0));
    return m;
}

const auto g_intProviders = buildIntProviders();
const auto g_modifiers = buildModifiers();

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind.empty()) return true;

    if (kind == "INT") {
        std::string name;
        long long seed;
        in >> name >> seed;
        auto it = g_intProviders.find(name);
        if (it == g_intProviders.end()) { err = "unknown int provider " + name; return false; }
        LegacyRandomSource r(seed);
        for (int i = 0; i < 8; ++i) {
            int32_t expected;
            in >> expected;
            const int32_t got = it->second->sample(r);
            if (got != expected) {
                err = "INT " + name + " seed " + std::to_string(seed) + " sample[" + std::to_string(i) +
                      "] " + std::to_string(got) + "!=" + std::to_string(expected);
                return false;
            }
        }
        return true;
    }

    if (kind == "POS") {
        std::string name;
        long long seed;
        int ox, oy, oz, count;
        std::string posStr;
        in >> name >> seed >> ox >> oy >> oz >> count >> posStr;
        auto it = g_modifiers.find(name);
        if (it == g_modifiers.end()) { err = "unknown modifier " + name; return false; }
        LegacyRandomSource r(seed);
        const std::vector<BlockPos> got = it->second->getPositions(nullptr, r, BlockPos{ ox, oy, oz });
        if (static_cast<int>(got.size()) != count) {
            err = "POS " + name + " seed " + std::to_string(seed) + " count " + std::to_string(got.size()) +
                  "!=" + std::to_string(count);
            return false;
        }
        // Build expected position string in the same format and compare.
        std::string gotStr;
        for (size_t i = 0; i < got.size(); ++i) {
            if (i) gotStr += ',';
            gotStr += std::to_string(got[i].x) + ":" + std::to_string(got[i].y) + ":" + std::to_string(got[i].z);
        }
        if (gotStr.empty()) gotStr = "-";
        if (gotStr != posStr) {
            err = "POS " + name + " seed " + std::to_string(seed) + " positions differ";
            return false;
        }
        return true;
    }

    return true;
}

// Hardcoded reference lines (seed 0) from PlacementParity.java.
const std::vector<std::string> kHardcoded = {
    "INT\tbias0_4\t0\t0\t2\t0\t0\t4\t2\t2\t0",
    "INT\tclamp_uni\t0\t6\t8\t0\t4\t5\t0\t3\t0",
    "INT\tuni0_7\t0\t5\t6\t1\t4\t5\t2\t4\t0",
    "INT\tuni1_3\t0\t1\t2\t2\t3\t3\t3\t3\t1",
    "INT\twl_19_1\t0\t0\t0\t0\t0\t0\t0\t0\t0",
    "INT\twl_mixed\t0\t2\t2\t1\t1\t2\t1\t2\t5",
    "POS\tcount_uni\t0\t100\t64\t-50\t4\t100:64:-50,100:64:-50,100:64:-50,100:64:-50",
    "POS\tcount_wl\t0\t100\t64\t-50\t0\t-",
    "POS\tinsquare\t0\t100\t64\t-50\t1\t111:64:-37",
    "POS\trarity7\t0\t100\t64\t-50\t0\t-",
    "POS\troff_h\t0\t100\t64\t-50\t1\t102:64:-51",
    "POS\troff_v\t0\t100\t64\t-50\t1\t100:62:-50",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n';
            }
        }
        std::cout << "placement cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    bool ok = true;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) {
            ok = false;
            std::cerr << "FAIL: " << err << '\n';
        }
    }
    if (!ok) {
        std::cerr << "Placement parity checks FAILED\n";
        return 1;
    }
    std::cout << "Placement parity checks passed\n";
    return 0;
}
