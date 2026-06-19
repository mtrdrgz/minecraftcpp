// Bit-exact parity gate for net.minecraft.world.level.levelgen.synth.NormalNoise.
//
// This test VERIFIES the EXISTING engine port mc::levelgen::NormalNoise
// (mcpp/src/world/level/levelgen/Noise.{h,cpp}); it does not duplicate it.
//
// NormalNoise.create(random, NoiseParameters(firstOctave, amplitudes)) builds two
// PerlinNoise from the SAME random consumed in sequence, and combines them with a
// value factor = 0.16666.../expectedDeviation(octaveSpan). getValue(x,y,z) =
// (first.getValue(x,y,z) + second.getValue(x*F,y*F,z*F)) * valueFactor,
// F = 1.0181268882175227.
//
// The test mirrors the identical NoiseParameters config table and seeds the
// identical RandomSource (LegacyRandomSource or XoroshiroRandomSource) as
// tools/NormalNoiseParity.java, then constructs the real engine NormalNoise and
// compares maxValue() and getValue(x,y,z) BIT-FOR-BIT.
//
// Usage: normal_noise_parity --cases <tsv>

#include "../RandomSource.h"
#include "../Noise.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::NoiseParameters;
using mc::levelgen::NormalNoise;
using mc::levelgen::RandomSource;
using mc::levelgen::XoroshiroRandomSource;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

// NoiseParameters config table — MUST match NormalNoiseParity.java CONFIGS exactly:
// each row is { firstOctave, amplitude0, amplitude1, ... }.
static const std::vector<std::vector<double>> kConfigs = {
    { 0.0, 1.0 },                                       // cfg 0
    { -1.0, 1.0, 1.0 },                                 // cfg 1
    { -3.0, 1.0, 1.0, 1.0, 1.0 },                       // cfg 2
    { -7.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 },   // cfg 3
    { -2.0, 1.0, 0.0, 1.0 },                            // cfg 4
    { -5.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0 },             // cfg 5
    { 3.0, 1.0 },                                       // cfg 6
    { -4.0, 1.5, 0.5, 2.0, 0.25 },                      // cfg 7
    { 0.0, 0.0, 1.0 },                                  // cfg 8
};

static NoiseParameters makeParams(int cfg) {
    const std::vector<double>& c = kConfigs[static_cast<size_t>(cfg)];
    NoiseParameters np;
    np.firstOctave = static_cast<int32_t>(c[0]);
    np.amplitudes.assign(c.begin() + 1, c.end());
    return np;
}

// Build the engine NormalNoise seeded identically to the Java GT.
// rngKind: 0 = LegacyRandomSource(seed), 1 = XoroshiroRandomSource(seed).
static NormalNoise makeNoise(int cfg, int rngKind, int64_t seed) {
    std::unique_ptr<RandomSource> rs;
    if (rngKind == 0) {
        rs = std::make_unique<LegacyRandomSource>(seed);
    } else {
        rs = std::make_unique<XoroshiroRandomSource>(seed);
    }
    return NormalNoise::create(*rs, makeParams(cfg));
}

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) {
        out.push_back(cur);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: normal_noise_parity --cases <tsv>\n");
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
        return 2;
    }

    // Cache one NormalNoise per (cfg, rng, seed) trio; the GT groups rows by trio.
    int cachedCfg = -1, cachedRng = -1;
    int64_t cachedSeed = 0;
    bool haveCached = false;
    std::unique_ptr<NormalNoise> cached;

    auto noiseFor = [&](int cfg, int rng, int64_t seed) -> NormalNoise& {
        if (!haveCached || cachedCfg != cfg || cachedRng != rng || cachedSeed != seed) {
            cached = std::make_unique<NormalNoise>(makeNoise(cfg, rng, seed));
            cachedCfg = cfg;
            cachedRng = rng;
            cachedSeed = seed;
            haveCached = true;
        }
        return *cached;
    };

    long long cases = 0;
    long long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> f = splitTabs(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "MAX") {
            // MAX cfg rng seed maxValue
            if (f.size() != 5) { ++mismatches; std::fprintf(stderr, "MAX bad arity\n"); continue; }
            int cfg = std::stoi(f[1]);
            int rng = std::stoi(f[2]);
            int64_t seed = std::stoll(f[3]);
            uint64_t expected = std::stoull(f[4], nullptr, 16);
            NormalNoise& n = noiseFor(cfg, rng, seed);
            double got = n.maxValue();
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr, "MAX mismatch cfg=%d rng=%d seed=%lld got %016llx exp %016llx\n",
                    cfg, rng, (long long)seed,
                    (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "VAL") {
            // VAL cfg rng seed x y z value
            if (f.size() != 8) { ++mismatches; std::fprintf(stderr, "VAL bad arity\n"); continue; }
            int cfg = std::stoi(f[1]);
            int rng = std::stoi(f[2]);
            int64_t seed = std::stoll(f[3]);
            double x = bd(f[4]), y = bd(f[5]), z = bd(f[6]);
            uint64_t expected = std::stoull(f[7], nullptr, 16);
            NormalNoise& n = noiseFor(cfg, rng, seed);
            double got = n.getValue(x, y, z);
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr,
                    "VAL mismatch cfg=%d rng=%d seed=%lld x=%016llx y=%016llx z=%016llx got %016llx exp %016llx\n",
                    cfg, rng, (long long)seed,
                    (unsigned long long)db(x), (unsigned long long)db(y), (unsigned long long)db(z),
                    (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        // Unknown tag -> count as mismatch so a malformed TSV cannot pass silently.
        ++mismatches;
        std::fprintf(stderr, "unknown tag '%s'\n", tag.c_str());
    }

    std::printf("NormalNoise cases=%lld mismatches=%lld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
