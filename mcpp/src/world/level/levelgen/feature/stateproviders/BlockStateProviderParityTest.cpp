// Parity test for BlockStateProvider.getState (Simple / Weighted). Ground truth:
// tools/BlockStateProviderParity.java (the real decompiled providers).
//
//   default        -> hardcoded self-checks
//   --cases <tsv>  -> verify every generated line

#include "../../RandomSource.h"
#include "BlockStateProvider.h"

#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::levelgen::feature::stateproviders;

namespace {

BlockStateProviderPtr weighted(std::vector<WeightedStateProvider::Entry> e) {
    return std::make_shared<WeightedStateProvider>(std::move(e));
}

std::map<std::string, BlockStateProviderPtr> buildProviders() {
    std::map<std::string, BlockStateProviderPtr> m;
    m["simple_grass"] = SimpleStateProvider::of("minecraft:short_grass");
    m["weighted"] = weighted({ { "minecraft:short_grass", 3 }, { "minecraft:fern", 1 }, { "minecraft:dandelion", 1 } });
    m["weighted2"] = weighted({ { "minecraft:poppy", 2 }, { "minecraft:dandelion", 2 }, { "minecraft:blue_orchid", 1 },
                                { "minecraft:allium", 1 }, { "minecraft:oxeye_daisy", 1 } });
    return m;
}

const auto g_providers = buildProviders();

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string kind;
    in >> kind;
    if (kind != "BSP") return true;
    std::string name;
    long long seed;
    in >> name >> seed;
    auto it = g_providers.find(name);
    if (it == g_providers.end()) { err = "unknown provider " + name; return false; }
    LegacyRandomSource r(seed);
    for (int i = 0; i < 8; ++i) {
        std::string expected;
        in >> expected;
        const std::string got = it->second->getState(r, BlockPos{ 10, 64, -7 });
        if (got != expected) {
            err = "BSP " + name + " seed " + std::to_string(seed) + "[" + std::to_string(i) + "] " + got + "!=" + expected;
            return false;
        }
    }
    return true;
}

const std::vector<std::string> kHardcoded = {
    "BSP\tsimple_grass\t0\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass\tminecraft:short_grass",
    "BSP\tweighted\t0\tminecraft:short_grass\tminecraft:fern\tminecraft:dandelion\tminecraft:short_grass\tminecraft:short_grass\tminecraft:fern\tminecraft:short_grass\tminecraft:short_grass",
    "BSP\tweighted2\t0\tminecraft:allium\tminecraft:dandelion\tminecraft:blue_orchid\tminecraft:dandelion\tminecraft:blue_orchid\tminecraft:poppy\tminecraft:dandelion\tminecraft:poppy",
    "BSP\tweighted2\t42\tminecraft:poppy\tminecraft:allium\tminecraft:oxeye_daisy\tminecraft:dandelion\tminecraft:allium\tminecraft:blue_orchid\tminecraft:poppy\tminecraft:dandelion",
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
            if (!verifyLine(line, err)) { ++bad; if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n'; }
        }
        std::cout << "block-state-provider cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }
    bool ok = true;
    for (const auto& line : kHardcoded) {
        std::string err;
        if (!verifyLine(line, err)) { ok = false; std::cerr << "FAIL: " << err << '\n'; }
    }
    if (!ok) { std::cerr << "BlockStateProvider parity checks FAILED\n"; return 1; }
    std::cout << "BlockStateProvider parity checks passed\n";
    return 0;
}
