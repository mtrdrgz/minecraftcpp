// Parity test for mc::levelgen::PerlinSimplexNoise (the existing engine port in
// world/level/levelgen/Noise.{h,cpp}) vs the REAL decompiled
// net.minecraft.world.level.levelgen.synth.PerlinSimplexNoise.
//
// This gate does NOT define a new PerlinSimplexNoise — it #includes the existing
// engine header and proves the certified code matches Java bit-for-bit. The ctor
// builds one octave SimplexNoise per octave value, each consuming a RandomSource
// (3 nextDouble + a 256-entry Fisher-Yates shuffle, or consumeCount(262) for unused
// octaves), and reseeds the high-freq octaves from a derived LegacyRandomSource. We
// rebuild each config with an identically seeded mc::levelgen::WorldgenRandom wrapping
// a LegacyRandomSource(seed) — the same `new WorldgenRandom(new LegacyRandomSource(
// seed))` the Java GT tool (and vanilla Biome.java) uses.
//
// Ground truth: mcpp/tools/PerlinSimplexNoiseParity.java. Doubles compared by raw
// IEEE bits via std::bit_cast (never by value).
//
//   --cases <tsv>  -> verify every line of the generated reference
//   default        -> a couple of jar-free self-consistency checks

#include "Noise.h"
#include "RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {

double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(line);
    while (std::getline(in, cur, '\t')) out.push_back(cur);
    return out;
}

std::vector<int32_t> parseOctaves(const std::string& csv) {
    std::vector<int32_t> out;
    std::string cur;
    std::istringstream in(csv);
    while (std::getline(in, cur, ',')) {
        if (!cur.empty()) out.push_back(std::stoi(cur));
    }
    return out;
}

// Build the engine PerlinSimplexNoise the same way vanilla does: wrap a seeded
// LegacyRandomSource in a WorldgenRandom (matches Biome.java's construction site,
// which is exactly what the Java GT tool replicates).
PerlinSimplexNoise makeNoise(int64_t seed, const std::vector<int32_t>& octaves) {
    WorldgenRandom rng(std::make_shared<LegacyRandomSource>(seed));
    return PerlinSimplexNoise(rng, octaves);
}

} // namespace

int main(int argc, char** argv) {
    if (argc <= 2 || std::string(argv[1]) != "--cases") {
        // Self-test: a single-octave noise produces finite values; the high-freq
        // reseed path runs without trouble for a mixed octave set.
        PerlinSimplexNoise n0 = makeNoise(1234, {0});
        PerlinSimplexNoise n1 = makeNoise(0, {-1, 0, 1});
        double a = n0.getValue(0.5, -0.5, false);
        double b = n0.getValue(0.5, -0.5, true);
        double c = n1.getValue(1.5, 2.25, true);
        bool ok = (a == a) && (b == b) && (c == c); // not NaN
        std::cout << "PerlinSimplexNoise self-test " << (ok ? "passed" : "FAILED") << '\n';
        return ok ? 0 : 1;
    }

    std::ifstream f(argv[2]);
    if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }

    // Cache one PerlinSimplexNoise per config (seed + octave set). The ctor consumes
    // the RNG, so we rebuild only when the config changes. The TSV groups all PV rows
    // of a config together (CFG row precedes them), so a single cached instance per
    // current config is sufficient and fast. PerlinSimplexNoise has no default ctor,
    // so the cache holds it through a unique_ptr.
    int64_t curSeed = 0;
    std::string curOct;
    std::unique_ptr<PerlinSimplexNoise> noise;

    auto ensureCfg = [&](int64_t seed, const std::string& octCsv) {
        if (!noise || seed != curSeed || octCsv != curOct) {
            noise = std::make_unique<PerlinSimplexNoise>(makeNoise(seed, parseOctaves(octCsv)));
            curSeed = seed;
            curOct = octCsv;
        }
    };

    std::string line;
    long n = 0, bad = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::vector<std::string> t = split(line);
        if (t.empty()) continue;
        const std::string& tag = t[0];

        if (tag == "PV") {
            // PV cfgIdx seed octCsv xBits yBits useStart valueBits
            ++n;
            int64_t seed = std::stoll(t[2]);
            const std::string& octCsv = t[3];
            ensureCfg(seed, octCsv);
            double x = bd(t[4]);
            double y = bd(t[5]);
            bool useStart = std::stoi(t[6]) != 0;
            uint64_t ev = std::stoull(t[7], nullptr, 16);
            uint64_t gv = db(noise->getValue(x, y, useStart));
            if (gv != ev) {
                ++bad;
                if (bad <= 20)
                    std::cerr << "MISMATCH PV cfg=" << t[1] << " seed=" << seed
                              << " oct=" << octCsv << " x=" << x << " y=" << y
                              << " useStart=" << useStart
                              << " got " << gv << " != " << ev << '\n';
            }
        }
        // CFG rows (and any unknown tags) are informational; skip silently.
    }

    std::cout << "PerlinSimplexNoise cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
