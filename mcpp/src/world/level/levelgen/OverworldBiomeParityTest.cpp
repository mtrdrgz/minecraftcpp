// Parity test for the overworld biome climate pipeline:
//   * OverworldBiomeBuilder  -> the 7593-entry climate parameter list
//   * Climate::RTree         -> the production findValue() biome search
//
// Ground truth comes from the real decompiled Minecraft 26.1.2 code via
// mcpp/tools/OverworldBiomeParity.java (see that file for how to regenerate).
//
// This executable has two layers:
//   1. Self-contained checks (default, no args): exact builder spot-checks,
//      interior unique-minimum biome lookups, and the R-tree exactness
//      invariant - findValue() must always return a global distance minimizer.
//      These need no Mojang files and run on Linux and Windows.
//   2. Full comparison (--cases <file>): replays every target in a TSV produced
//      by OverworldBiomeParity.java and requires an exact biome match, including
//      the distance-tie cases that depend on R-tree traversal order and the
//      lastResult seed. This is the strongest check and needs the local jar.

#include "Climate.h"
#include "OverworldBiomeBuilder.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

using Entry = mc::levelgen::Climate::ParameterList<std::string>::Entry;

bool g_ok = true;

void check(bool cond, const std::string& label) {
    if (!cond) {
        g_ok = false;
        std::cerr << "FAIL: " << label << '\n';
    }
}

// Deterministic, cross-platform LCG so the sampled targets are identical
// everywhere (it does not need to match any Java RNG).
struct Lcg {
    std::uint64_t s;
    explicit Lcg(std::uint64_t seed) : s(seed) {}
    std::uint64_t next() {
        s = s * 6364136223846793005ULL + 1442695040888963407ULL;
        return s;
    }
    std::int64_t range(std::int64_t lo, std::int64_t hi) {
        const std::uint64_t span = static_cast<std::uint64_t>(hi - lo + 1);
        return lo + static_cast<std::int64_t>((next() >> 33) % span);
    }
};

struct ParamRow {
    std::size_t index;
    std::int64_t v[13];
    const char* biome;
};

void checkParamRow(const std::vector<Entry>& list, const ParamRow& row) {
    if (row.index >= list.size()) {
        check(false, "param row index out of range");
        return;
    }
    const auto& p = list[row.index].first;
    const std::int64_t got[13] = {
        p.temperature.min, p.temperature.max, p.humidity.min, p.humidity.max,
        p.continentalness.min, p.continentalness.max, p.erosion.min, p.erosion.max,
        p.depth.min, p.depth.max, p.weirdness.min, p.weirdness.max, p.offset
    };
    bool same = list[row.index].second == row.biome;
    for (int i = 0; i < 13; ++i) {
        same = same && got[i] == row.v[i];
    }
    check(same, "param row " + std::to_string(row.index) + " (" + row.biome + ")");
}

int runFullComparison(const std::vector<Entry>& list, const char* path) {
    mc::levelgen::Climate::ParameterList<std::string> pl(list);
    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open cases file: " << path << '\n';
        return 2;
    }
    std::string line;
    long n = 0;
    long mismatches = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        long long t, h, c, e, d, w;
        char biome[128];
        if (std::sscanf(line.c_str(), "%lld\t%lld\t%lld\t%lld\t%lld\t%lld\t%127s",
                        &t, &h, &c, &e, &d, &w, biome) != 7) {
            std::cerr << "parse error: " << line << '\n';
            return 3;
        }
        const std::string& got = pl.findValue(mc::levelgen::Climate::TargetPoint{ t, h, c, e, d, w });
        ++n;
        if (got != biome) {
            ++mismatches;
            if (mismatches <= 20) {
                std::cerr << "MISMATCH @" << n << ": got " << got << " expected " << biome << '\n';
            }
        }
    }
    std::cout << "full comparison: cases=" << n << " mismatches=" << mismatches << '\n';
    return mismatches == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    using namespace mc::levelgen;

    const std::vector<Entry> list = buildOverworldBiomePreset();

    if (argc > 2 && std::string(argv[1]) == "--cases") {
        return runFullComparison(list, argv[2]);
    }

    // (1) Builder: exact parameter-list size and spot rows (validates the
    //     OverworldBiomeBuilder port against the real Java output, 1:1).
    check(list.size() == 7593, "parameter count == 7593 (got " + std::to_string(list.size()) + ")");

    const ParamRow rows[] = {
        { 0,    { -10000, 10000, -10000, 10000, -12000, -10500, -10000, 10000, 0, 0, -10000, 10000, 0 }, "minecraft:mushroom_fields" },
        { 6999, { -10000, -4500, -1000, 1000, -1900, 300, -2225, 500, 10000, 10000, 9333, 10000, 0 },     "minecraft:snowy_taiga" },
        { 7592, { -10000, 10000, -10000, 10000, -10000, 10000, -10000, -3750, 11000, 11000, -10000, 10000, 0 }, "minecraft:deep_dark" },
    };
    for (const auto& r : rows) {
        checkParamRow(list, r);
    }

    // (2) Interior unique-minimum targets: each sits strictly inside exactly one
    //     parameter box, so the answer is unambiguous and independent of the
    //     R-tree's tie-break / lastResult behaviour.
    struct InteriorCase {
        std::int64_t t, h, c, e, d, w;
        const char* biome;
    };
    const InteriorCase interior[] = {
        { -7000, 0, -7500, 0, 0, 0, "minecraft:deep_frozen_ocean" },
        { 8000, 0, -7500, 0, 0, 0, "minecraft:warm_ocean" },
        { 0, 0, -11000, 0, 0, 0, "minecraft:mushroom_fields" },
        { 0, 0, 0, -9000, 11000, 0, "minecraft:deep_dark" },
        { 0, 9000, 0, 0, 5000, 0, "minecraft:lush_caves" },
        { 0, 0, 9000, 0, 5000, 0, "minecraft:dripstone_caves" },
    };
    for (const auto& ic : interior) {
        Climate::ParameterList<std::string> pl(list); // fresh tree => no lastResult carryover
        const std::string& got = pl.findValue(Climate::TargetPoint{ ic.t, ic.h, ic.c, ic.e, ic.d, ic.w });
        check(got == ic.biome, std::string("interior case -> ") + ic.biome + " (got " + got + ")");
    }

    // (3) R-tree exactness invariant over a deterministic sample: findValue()
    //     must always return a biome whose parameter point achieves the global
    //     minimum fitness (brute force). Also confirm the R-tree genuinely
    //     diverges from naive first-minimum order (i.e. the fix is active).
    Climate::ParameterList<std::string> pl(list);
    Lcg rng(0x9E3779B97F4A7C15ULL);
    const int samples = 50000;
    int notMinimizer = 0;
    int divergesFromFirstMin = 0;
    for (int i = 0; i < samples; ++i) {
        Climate::TargetPoint tp{
            rng.range(-13000, 13000), rng.range(-13000, 13000), rng.range(-13000, 13000),
            rng.range(-13000, 13000), rng.range(-13000, 13000), rng.range(-13000, 13000)
        };
        const std::string& rtreeBiome = pl.findValue(tp);

        std::int64_t minFitness = std::numeric_limits<std::int64_t>::max();
        const std::string* firstMinBiome = nullptr;
        for (const auto& entry : list) {
            const std::int64_t f = entry.first.fitness(tp);
            if (f < minFitness) {
                minFitness = f;
                firstMinBiome = &entry.second;
            }
        }
        // Is the R-tree result one of the global minimizers?
        bool isMinimizer = false;
        for (const auto& entry : list) {
            if (entry.second == rtreeBiome && entry.first.fitness(tp) == minFitness) {
                isMinimizer = true;
                break;
            }
        }
        if (!isMinimizer) ++notMinimizer;
        if (firstMinBiome && rtreeBiome != *firstMinBiome) ++divergesFromFirstMin;
    }
    check(notMinimizer == 0, "R-tree always returns a global distance minimizer (failures: " + std::to_string(notMinimizer) + ")");
    check(divergesFromFirstMin > 0, "R-tree path diverges from first-minimum order (count: " + std::to_string(divergesFromFirstMin) + ")");
    std::cout << "sampled " << samples << " targets; R-tree vs first-minimum divergences: " << divergesFromFirstMin << '\n';

    if (!g_ok) {
        std::cerr << "OverworldBiome parity checks FAILED\n";
        return 1;
    }
    std::cout << "OverworldBiome parity checks passed\n";
    return 0;
}
