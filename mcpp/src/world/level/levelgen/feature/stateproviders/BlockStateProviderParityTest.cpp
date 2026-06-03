// Parity test for BlockStateProvider.getState (Simple / Weighted). Ground truth:
// tools/BlockStateProviderParity.java (the real decompiled providers).
//
//   default        -> hardcoded self-checks
//   --cases <tsv>  -> verify every generated line

#include "../../Noise.h"
#include "../../RandomSource.h"
#include "BlockStateProvider.h"
#include "NoiseBasedStateProviders.h"

#include <bit>
#include <cstdint>
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

NormalNoise makeFlowerNoise() {
    auto src = std::make_shared<LegacyRandomSource>(2345);
    WorldgenRandom wr(src);
    return NormalNoise::create(wr, NoiseParameters{ 0, { 1.0 } });
}

const NormalNoise g_noise = makeFlowerNoise();
const double kScale = 0.005f; // (double)(0.005f)

const NoiseThresholdProvider g_flowers(
    2345, NoiseParameters{ 0, { 1.0 } }, 0.005f, -0.8f, 0.33333334f, "minecraft:dandelion",
    { "minecraft:orange_tulip", "minecraft:red_tulip", "minecraft:pink_tulip", "minecraft:white_tulip" },
    { "minecraft:poppy", "minecraft:azure_bluet", "minecraft:oxeye_daisy", "minecraft:cornflower" });

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
    if (kind == "NOISE") {
        long long px, py, pz, expectedBits;
        in >> px >> py >> pz >> expectedBits;
        const double v = g_noise.getValue(px * kScale, py * kScale, pz * kScale);
        const long long got = std::bit_cast<std::int64_t>(v);
        if (got != expectedBits) {
            err = "NOISE " + std::to_string(px) + "," + std::to_string(py) + "," + std::to_string(pz) +
                  " bits " + std::to_string(got) + "!=" + std::to_string(expectedBits);
            return false;
        }
        return true;
    }
    if (kind == "BSPN") {
        long long px, py, pz, seed;
        std::string expected;
        in >> px >> py >> pz >> seed >> expected;
        LegacyRandomSource r(seed);
        const std::string got = g_flowers.getState(r, BlockPos{ static_cast<int>(px), static_cast<int>(py), static_cast<int>(pz) });
        if (got != expected) {
            err = "BSPN " + std::to_string(px) + "," + std::to_string(pz) + " seed " + std::to_string(seed) +
                  " " + got + "!=" + expected;
            return false;
        }
        return true;
    }
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
