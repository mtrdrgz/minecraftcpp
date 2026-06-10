// Parity test for net.minecraft.util.valueproviders.UniformFloat.
//
// VERIFIES the existing certified C++ port:
//   mc::valueproviders::UniformFloat  (in world/level/levelgen/FloatProvider.h)
//   UniformFloat::sample == mthRandomBetween == nextFloat()*(max-min)+min
//
// Ground truth: tools/UniformFloatParity.java drives the REAL UniformFloat over a
// battery of (min,max) ranges and seeds, against two certified RNGs seeded
// IDENTICALLY here (LegacyRandomSource / XoroshiroRandomSource). Each row carries
// 8 consecutive samples; we recompute and compare BIT-FOR-BIT.
//
//   default        -> a couple of hardcoded self-checks
//   --cases <tsv>  -> verify every UF row of the generated reference

#include "../../world/level/levelgen/FloatProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::RandomSource;
using mc::levelgen::XoroshiroRandomSource;
using mc::valueproviders::UniformFloat;

namespace {

// hex (%08x) -> raw float bits -> float
float bitsHexToFloat(const std::string& hex) {
    const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    return std::bit_cast<float>(bits);
}

std::unique_ptr<RandomSource> makeRng(const std::string& rng, int64_t seed) {
    if (rng == "LEG") return std::make_unique<LegacyRandomSource>(seed);
    if (rng == "XOR") return std::make_unique<XoroshiroRandomSource>(seed);
    return nullptr;
}

// Returns false on mismatch; fills err.
bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;
    if (tag != "UF") return true; // ignore foreign tags

    std::string rng, seedStr, minHex, maxHex;
    in >> rng >> seedStr >> minHex >> maxHex;
    if (maxHex.empty()) { err = "malformed UF row"; return false; }

    const int64_t seed = static_cast<int64_t>(std::stoll(seedStr));
    const float min = bitsHexToFloat(minHex);
    const float max = bitsHexToFloat(maxHex);

    auto r = makeRng(rng, seed);
    if (!r) { err = "unknown rng " + rng; return false; }

    UniformFloat uf(min, max);

    for (int i = 0; i < 8; ++i) {
        std::string expHex;
        if (!(in >> expHex)) { err = "too few samples in UF row"; return false; }
        const int32_t expected = std::bit_cast<int32_t>(bitsHexToFloat(expHex));
        const int32_t got = std::bit_cast<int32_t>(uf.sample(*r));
        if (got != expected) {
            err = "UF " + rng + " seed " + seedStr + " min " + minHex + " max " + maxHex +
                  " [" + std::to_string(i) + "] bits " + std::to_string(got) +
                  "!=" + std::to_string(expected);
            return false;
        }
    }
    return true;
}

// A couple of hardcoded rows (verified against the GT generator) so the test is
// meaningful even with no TSV. Bits are %08x of Float.floatToRawIntBits.
const std::vector<std::string> kHardcoded = {
    // LEG seed 0, [0,1): identical to the certified HeightFloatProvider uf01 row
    // (FloatProvider parity, decimal bits converted to %08x).
    "UF\tLEG\t0\t00000000\t3f800000\t3f3b20b4\t3f54d951\t3e764f2c\t3f1b3970\t3f232dc9\t3e9e3be0\t3f0ce970\t3defa128",
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
        std::cout << "UniformFloat cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    long n = 0, bad = 0;
    for (const auto& line : kHardcoded) {
        std::string err;
        ++n;
        if (!verifyLine(line, err)) { ++bad; std::cerr << "MISMATCH: " << err << '\n'; }
    }
    std::cout << "UniformFloat cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
