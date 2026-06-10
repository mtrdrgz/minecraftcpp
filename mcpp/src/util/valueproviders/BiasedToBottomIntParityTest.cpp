// Parity test for net.minecraft.util.valueproviders.BiasedToBottomInt.
//
// VERIFIES the existing certified engine port:
//   mc::valueproviders::BiasedToBottomInt  (world/level/levelgen/IntProvider.h)
// against ground truth from the REAL decompiled class, emitted by
//   mcpp/tools/BiasedToBottomIntParity.java  -> biased_to_bottom_int.tsv
//
//   default        -> a couple of hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every B2B row of the generated reference
//
// Row format (tab-separated):
//   B2B  <min>  <max>  <seed>  <s0..s7>
// One LegacyRandomSource is seeded with <seed> and sampled 8 times in sequence
// (no reseed between draws) — exactly mirroring the GT tool, so the RNG stream
// advances identically. sample() is min + nextInt(nextInt(max-min+1)+1), with the
// inner nextInt drawn before the outer (fixed JVM arg-eval order, matched by the
// C++ port). Ints compared exactly.

#include "../../world/level/levelgen/IntProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::valueproviders::BiasedToBottomInt;

namespace {

// Draw 8 samples from a single continuously-advancing RNG seeded with `seed`.
void sample8(int32_t min, int32_t max, int64_t seed, int32_t out[8]) {
    BiasedToBottomInt provider(min, max);
    LegacyRandomSource r(seed);
    for (int i = 0; i < 8; ++i) {
        out[i] = provider.sample(r);
    }
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;
    if (tag != "B2B") return true; // ignore unknown tags

    int32_t min = 0, max = 0;
    long long seed = 0;
    in >> min >> max >> seed;

    int32_t got[8];
    sample8(min, max, static_cast<int64_t>(seed), got);

    for (int i = 0; i < 8; ++i) {
        int32_t expected = 0;
        in >> expected;
        if (got[i] != expected) {
            err = "B2B min=" + std::to_string(min) + " max=" + std::to_string(max) +
                  " seed=" + std::to_string(seed) + "[" + std::to_string(i) + "] " +
                  std::to_string(got[i]) + "!=" + std::to_string(expected);
            return false;
        }
    }
    return true;
}

// Self-checks: copied from a GT run (real BiasedToBottomInt + LegacyRandomSource).
const std::vector<std::string> kHardcoded = {
    // degenerate range [0-0]: always 0
    "B2B\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0\t0",
    // degenerate non-zero [3-3]: always min (3)
    "B2B\t3\t3\t42\t3\t3\t3\t3\t3\t3\t3\t3",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) {
            std::cerr << "cannot open " << argv[2] << '\n';
            return 2;
        }
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
        std::cout << "BiasedToBottomInt cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    long n = 0, bad = 0;
    for (const auto& line : kHardcoded) {
        std::string err;
        ++n;
        if (!verifyLine(line, err)) {
            ++bad;
            std::cerr << "FAIL: " << err << '\n';
        }
    }
    std::cout << "BiasedToBottomInt cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
