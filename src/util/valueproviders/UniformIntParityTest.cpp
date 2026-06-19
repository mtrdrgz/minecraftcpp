// Parity GATE for net.minecraft.util.valueproviders.UniformInt.
//
// Verifies the existing certified port mc::valueproviders::UniformInt in
// world/level/levelgen/IntProvider.h against ground truth from
// tools/UniformIntParity.java (the REAL decompiled UniformInt). No new port is
// written here: UniformInt.sample / minInclusive (getMinValue) / maxInclusive
// (getMaxValue) are exercised against both verified RandomSource flavours,
// seeded identically on both sides.
//
//   default        -> a couple of hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference, bit-exact
//
// Build target: uniform_int_parity   TSV: uniform_int.tsv

#include "../../world/level/levelgen/IntProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

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
using namespace mc::valueproviders;

namespace {

// Same provider set as tools/UniformIntParity.java, by name.
std::map<std::string, IntProviderPtr> buildProviders() {
    return {
        { "uni1_3", UniformInt::of(1, 3) },
        { "uni0_7", UniformInt::of(0, 7) },
        { "uni0_0", UniformInt::of(0, 0) },
        { "uni5_5", UniformInt::of(5, 5) },
        { "uni_neg", UniformInt::of(-5, 10) },
        { "uni_allneg", UniformInt::of(-12, -3) },
        { "uni0_255", UniformInt::of(0, 255) },
        { "uni0_99", UniformInt::of(0, 99) },
        { "uni1_1000", UniformInt::of(1, 1000) },
        { "uni_big", UniformInt::of(-32768, 32767) },
    };
}

std::shared_ptr<RandomSource> makeRng(int kind, int64_t seed) {
    if (kind == 0) return std::make_shared<LegacyRandomSource>(seed);
    return std::make_shared<XoroshiroRandomSource>(seed);
}

// Bit-exact integer comparison via std::bit_cast (here int32_t is already an
// exact representation; bit_cast keeps the contract uniform with float gates).
bool sameBits(int32_t a, int32_t b) {
    return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

bool verifyLine(const std::string& line,
                const std::map<std::string, IntProviderPtr>& providers,
                std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;

    if (tag == "META") {
        std::string name;
        int32_t expMin = 0, expMax = 0;
        in >> name >> expMin >> expMax;
        auto it = providers.find(name);
        if (it == providers.end()) { err = "unknown provider " + name; return false; }
        const int32_t gotMin = it->second->minInclusive();
        const int32_t gotMax = it->second->maxInclusive();
        if (!sameBits(gotMin, expMin)) {
            err = "META " + name + " minInclusive " + std::to_string(gotMin) +
                  "!=" + std::to_string(expMin);
            return false;
        }
        if (!sameBits(gotMax, expMax)) {
            err = "META " + name + " maxInclusive " + std::to_string(gotMax) +
                  "!=" + std::to_string(expMax);
            return false;
        }
        return true;
    }

    if (tag == "SAMPLE") {
        std::string name;
        int kind = 0;
        long long seed = 0;
        in >> name >> kind >> seed;
        auto it = providers.find(name);
        if (it == providers.end()) { err = "unknown provider " + name; return false; }
        auto rng = makeRng(kind, static_cast<int64_t>(seed));
        for (int i = 0; i < 8; ++i) {
            int32_t expected = 0;
            in >> expected;
            const int32_t got = it->second->sample(*rng);
            if (!sameBits(got, expected)) {
                err = "SAMPLE " + name + " rng" + std::to_string(kind) + " seed " +
                      std::to_string(seed) + " s[" + std::to_string(i) + "] " +
                      std::to_string(got) + "!=" + std::to_string(expected);
                return false;
            }
        }
        return true;
    }

    err = "unknown tag " + tag;
    return false;
}

int runSelfChecks() {
    // Spot checks: sample == nextInt(max-min+1)+min on the same seed; accessors.
    auto p = UniformInt::of(3, 9);
    if (p->minInclusive() != 3 || p->maxInclusive() != 9) {
        std::cout << "UniformIntParity self-check FAILED: accessors\n";
        return 1;
    }
    LegacyRandomSource a(42), b(42);
    const int32_t viaProvider = p->sample(a);
    const int32_t viaFormula = b.nextInt(9 - 3 + 1) + 3;
    if (viaProvider != viaFormula) {
        std::cout << "UniformIntParity self-check FAILED: sample formula\n";
        return 1;
    }
    std::cout << "UniformIntParity self-checks passed\n";
    return 0;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }

    if (casesPath.empty()) {
        return runSelfChecks();
    }

    std::ifstream f(casesPath);
    if (!f) {
        std::cout << "UniformIntParity cases=0 mismatches=1 (cannot open " << casesPath << ")\n";
        return 1;
    }

    const auto providers = buildProviders();
    std::string line;
    int cases = 0, mismatches = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::string err;
        ++cases;
        if (!verifyLine(line, providers, err)) {
            ++mismatches;
            if (mismatches <= 20) std::cout << "  MISMATCH: " << err << "\n";
        }
    }

    std::cout << "UniformIntParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
