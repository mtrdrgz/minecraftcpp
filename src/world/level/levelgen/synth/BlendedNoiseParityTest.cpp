// Bit-exact parity gate for net.minecraft.world.level.levelgen.synth.BlendedNoise.
//
// This test VERIFIES the EXISTING engine port mc::levelgen::BlendedNoise
// (mcpp/src/world/level/levelgen/Noise.{h,cpp}); it does not duplicate it.
// BlendedNoise is the main terrain density noise: a minLimit/maxLimit/mainNoise blend
// (3 legacy Perlin stacks). It is constructed via the @VisibleForTesting public ctor
//   BlendedNoise(RandomSource, xzScale, yScale, xzFactor, yFactor, smearScaleMultiplier)
// which the C++ port mirrors as
//   BlendedNoise(std::shared_ptr<RandomSource>, xzScale, yScale, xzFactor, yFactor, smear).
// We seed mc::levelgen::XoroshiroRandomSource(seed) identically to the Java GT (which
// uses net.minecraft XoroshiroRandomSource(seed)), build the same BlendedNoise per config,
// and compare compute(x,y,z)/maxValue()/minValue() BIT-FOR-BIT against ground truth from
// tools/BlendedNoiseParity.java.
//
// The (seed, params) configs here MUST match BlendedNoiseParity.java's SEEDS/PARAMS tables
// exactly, since the TSV indexes configs by cfgIdx.
//
// Usage: blended_noise_parity --cases <tsv>

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

using mc::levelgen::BlendedNoise;
using mc::levelgen::XoroshiroRandomSource;
using mc::levelgen::RandomSource;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) {
        out.push_back(cur);
    }
    return out;
}

// MUST mirror BlendedNoiseParity.java SEEDS/PARAMS exactly (indexed by cfgIdx).
struct Config {
    int64_t seed;
    double xzScale, yScale, xzFactor, yFactor, smear;
};
static const Config CONFIGS[] = {
    {              0LL, 1.0,  1.0,  80.0,  80.0,  8.0 }, // overworld default
    {              1LL, 1.0,  1.0,  80.0,  80.0,  8.0 },
    {             42LL, 1.0,  1.0,  80.0,  80.0,  8.0 },
    {          12345LL, 1.0,  1.0,  80.0,  80.0,  8.0 },
    {     -987654321LL, 1.0,  1.0,  80.0,  80.0,  8.0 },
    {  9876543210123LL, 1.0,  1.0,  80.0,  80.0,  8.0 },
    {              7LL, 0.25, 0.5,  40.0,  20.0,  4.0 },
    {              7LL, 2.0,  3.0, 120.0,  90.0,  1.0 },
    {         100000LL, 0.5,  0.5,  60.0,  60.0,  6.0 },
    {     0x5DEECE66DLL, 1.0,  1.0,  80.0,  80.0,  8.0 },
};
static constexpr int NUM_CONFIGS = static_cast<int>(sizeof(CONFIGS) / sizeof(CONFIGS[0]));

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: blended_noise_parity --cases <tsv>\n");
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
        return 2;
    }

    // Build one BlendedNoise per config, seeding XoroshiroRandomSource identically to the
    // Java GT. The ctor consumes the random in the same order (3x createLegacyForBlendedNoise).
    std::vector<std::unique_ptr<BlendedNoise>> noises;
    noises.reserve(NUM_CONFIGS);
    for (int c = 0; c < NUM_CONFIGS; ++c) {
        const Config& cfg = CONFIGS[c];
        auto rng = std::make_shared<XoroshiroRandomSource>(cfg.seed);
        noises.push_back(std::make_unique<BlendedNoise>(
            std::static_pointer_cast<RandomSource>(rng),
            cfg.xzScale, cfg.yScale, cfg.xzFactor, cfg.yFactor, cfg.smear));
    }

    long long cases = 0;
    long long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> f = splitTabs(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "MAX" || tag == "MIN") {
            // MAX/MIN cfgIdx value
            if (f.size() != 3) { ++mismatches; continue; }
            int cfg = std::stoi(f[1]);
            if (cfg < 0 || cfg >= NUM_CONFIGS) { ++mismatches; continue; }
            uint64_t expected = std::stoull(f[2], nullptr, 16);
            double got = (tag == "MAX") ? noises[cfg]->maxValue() : noises[cfg]->minValue();
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr, "%s mismatch cfg=%d got %016llx exp %016llx\n",
                    tag.c_str(), cfg,
                    (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "COMPUTE") {
            // COMPUTE cfgIdx blockX blockY blockZ value
            if (f.size() != 6) { ++mismatches; continue; }
            int cfg = std::stoi(f[1]);
            if (cfg < 0 || cfg >= NUM_CONFIGS) { ++mismatches; continue; }
            int bx = std::stoi(f[2]);
            int by = std::stoi(f[3]);
            int bz = std::stoi(f[4]);
            uint64_t expected = std::stoull(f[5], nullptr, 16);
            double got = noises[cfg]->compute(bx, by, bz);
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr, "COMPUTE mismatch cfg=%d xyz=%d,%d,%d got %016llx exp %016llx\n",
                    cfg, bx, by, bz,
                    (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        // Unknown tag -> count as mismatch so a malformed TSV cannot pass silently.
        ++mismatches;
        std::fprintf(stderr, "unknown tag '%s'\n", tag.c_str());
    }

    std::printf("BlendedNoise cases=%lld mismatches=%lld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
