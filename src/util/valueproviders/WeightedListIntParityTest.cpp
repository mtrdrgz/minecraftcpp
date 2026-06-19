// Bit-exact parity GATE for net.minecraft.util.valueproviders.WeightedListInt.
//
// VERIFIES the existing engine port at
//   mcpp/src/world/level/levelgen/IntProvider.h  (mc::valueproviders::WeightedListInt)
// against ground truth produced by the REAL decompiled class via
//   mcpp/tools/WeightedListIntParity.java  ->  weighted_list_int.tsv
//
// WeightedListInt.sample(random) == distribution.getRandomOrThrow(random).sample(random):
//   selection = random.nextInt(totalWeight); walk cumulative weights -> chosen
//   sub-provider; then sub-provider.sample(random). The Java WeightedList uses a
//   Flat array (totalWeight < 64) or a Compact cumulative walk (>= 64); both map
//   the same `selection` index to the same entry, so the single C++ cumulative
//   walk must agree across BOTH regimes. The TSV deliberately straddles 64.
//
// RandomSource is mc::levelgen::LegacyRandomSource (already certified 1:1),
// re-seeded per case identically to the Java LegacyRandomSource.
//
//   default        -> built-in self-checks (no Mojang files needed)
//   --cases <tsv>  -> verify every WLI/META line of the generated reference
//
// Distributions below MUST mirror WeightedListIntParity.java entry-for-entry and
// in the SAME ORDER (the cumulative walk is order-sensitive).

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

using mc::levelgen::LegacyRandomSource;
using namespace mc::valueproviders;

namespace {

IntProviderPtr wl(std::vector<WeightedListInt::Entry> entries) {
    return std::make_shared<WeightedListInt>(std::move(entries));
}

// Mirror of WeightedListIntParity.java distributions(), same names + same order.
std::map<std::string, IntProviderPtr> buildDistributions() {
    std::map<std::string, IntProviderPtr> m;

    // --- Flat regime (totalWeight < 64) ---
    m["wl_19_1"] = wl({ { ConstantInt::of(0), 19 }, { ConstantInt::of(1), 1 } });
    m["wl_c3"]   = wl({ { ConstantInt::of(7), 1 }, { ConstantInt::of(8), 2 }, { ConstantInt::of(9), 3 } });
    m["wl_mixed"] = wl({ { UniformInt::of(1, 2), 3 }, { ConstantInt::of(5), 1 } });
    m["wl_het"] = wl({
        { ConstantInt::of(-3), 4 },
        { UniformInt::of(0, 7), 3 },
        { BiasedToBottomInt::of(0, 4), 2 },
        { ClampedInt::of(UniformInt::of(-5, 10), 0, 8), 1 },
    });
    m["wl_flat63"] = wl({
        { ConstantInt::of(100), 60 },
        { UniformInt::of(200, 205), 3 },
    });

    // --- Compact regime (totalWeight >= 64) ---
    m["wl_compact64"] = wl({
        { ConstantInt::of(100), 60 },
        { UniformInt::of(200, 205), 4 },
    });
    m["wl_compact_big"] = wl({
        { ConstantInt::of(-10), 50 },
        { UniformInt::of(1, 6), 40 },
        { TrapezoidInt::of(-8, 8, 0), 25 },
        { BiasedToBottomInt::of(2, 9), 15 },
    });
    m["wl_compact200"] = wl({
        { ConstantInt::of(0), 199 },
        { ConstantInt::of(1), 1 },
    });

    return m;
}

const std::map<std::string, IntProviderPtr>& distributions() {
    static const std::map<std::string, IntProviderPtr> m = buildDistributions();
    return m;
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;

    if (tag == "META") {
        std::string name;
        long long unused;
        int32_t expMin, expMax;
        in >> name >> unused >> expMin >> expMax;
        const auto it = distributions().find(name);
        if (it == distributions().end()) { err = "unknown distribution " + name; return false; }
        const int32_t gotMin = it->second->minInclusive();
        const int32_t gotMax = it->second->maxInclusive();
        if (gotMin != expMin || gotMax != expMax) {
            err = "META " + name + " min/max " + std::to_string(gotMin) + "/" + std::to_string(gotMax) +
                  " != " + std::to_string(expMin) + "/" + std::to_string(expMax);
            return false;
        }
        return true;
    }

    if (tag == "WLI") {
        std::string name;
        long long seed;
        in >> name >> seed;
        const auto it = distributions().find(name);
        if (it == distributions().end()) { err = "unknown distribution " + name; return false; }
        LegacyRandomSource r(static_cast<int64_t>(seed));
        for (int i = 0; i < 8; ++i) {
            int32_t expected;
            in >> expected;
            const int32_t got = it->second->sample(r);
            // Bit-for-bit on the 32-bit sample.
            if (std::bit_cast<uint32_t>(got) != std::bit_cast<uint32_t>(expected)) {
                err = "WLI " + name + " seed " + std::to_string(seed) + " sample[" + std::to_string(i) +
                      "] " + std::to_string(got) + " != " + std::to_string(expected);
                return false;
            }
        }
        return true;
    }

    err = "unknown tag " + tag;
    return false;
}

int runCases(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "WeightedListIntParity: cannot open " << path << "\n";
        return 2;
    }
    std::string line;
    int cases = 0, mismatches = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++cases;
        std::string err;
        if (!verifyLine(line, err)) {
            ++mismatches;
            if (mismatches <= 20) std::cerr << "MISMATCH: " << err << "\n";
        }
    }
    std::cout << "WeightedListIntParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}

// Self-checks independent of Mojang: confirm the cumulative-walk maps a given
// selection index to the same entry a Flat array would, for both regimes, and
// that min/max aggregate over all entries.
int runSelfChecks() {
    int fails = 0;

    // wl_19_1: totalWeight 20. nextInt(20) in [0,18] -> entry0 (value 0),
    // 19 -> entry1 (value 1). Use a fake deterministic walk via known seeds is
    // overkill here; instead validate min/max and that sampling never throws.
    const auto& d = distributions();

    auto checkMinMax = [&](const std::string& n, int32_t mn, int32_t mx) {
        const auto it = d.find(n);
        if (it == d.end()) { std::cerr << "self: missing " << n << "\n"; ++fails; return; }
        if (it->second->minInclusive() != mn || it->second->maxInclusive() != mx) {
            std::cerr << "self: " << n << " min/max " << it->second->minInclusive() << "/"
                      << it->second->maxInclusive() << " expected " << mn << "/" << mx << "\n";
            ++fails;
        }
    };
    checkMinMax("wl_19_1", 0, 1);
    checkMinMax("wl_c3", 7, 9);
    checkMinMax("wl_mixed", 1, 5);
    // wl_het entries minInclusive = {-3, 0, 0, max(0,-5)=0} -> -3 ;
    //       maxInclusive = {-3, 7, 4, min(8,10)=8} -> 8
    checkMinMax("wl_het", -3, 8);
    checkMinMax("wl_flat63", 100, 205);
    checkMinMax("wl_compact64", 100, 205);
    // wl_compact_big: mins {-10,1,-8,2}->-10 ; maxs {-10,6,8,9}->9
    checkMinMax("wl_compact_big", -10, 9);
    checkMinMax("wl_compact200", 0, 1);

    // Sampling determinism: same seed -> same sequence; and value within [min,max].
    for (const auto& [name, p] : d) {
        LegacyRandomSource a(12345), b(12345);
        for (int i = 0; i < 32; ++i) {
            const int32_t va = p->sample(a);
            const int32_t vb = p->sample(b);
            if (va != vb) { std::cerr << "self: nondeterministic " << name << "\n"; ++fails; break; }
            if (va < p->minInclusive() || va > p->maxInclusive()) {
                std::cerr << "self: out-of-range " << name << " " << va << "\n"; ++fails; break;
            }
        }
    }

    std::cout << "WeightedListIntParity self-checks fails=" << fails << "\n";
    return fails == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath.empty()) return runCases(casesPath);
    return runSelfChecks();
}
