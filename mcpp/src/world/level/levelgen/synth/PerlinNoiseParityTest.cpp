// Bit-exact parity gate for net.minecraft.world.level.levelgen.synth.PerlinNoise.
//
// This test VERIFIES the EXISTING engine port mc::levelgen::PerlinNoise
// (mcpp/src/world/level/levelgen/Noise.{h,cpp}); it does not duplicate it.
//
// Each TSV row carries a construction <tag> identifying the exact recipe used to
// build the PerlinNoise on the Java side. The C++ side parses that tag and rebuilds
// the IDENTICAL PerlinNoise via the matching engine factory, seeding the same
// RandomSource (Xoroshiro for new-init create(), Legacy for the createLegacy* paths)
// with the same seed. Then it compares getValue / getValue(...,yScale,yFudge) /
// maxValue / maxBrokenValue BIT-FOR-BIT against ground truth from
// tools/PerlinNoiseParity.java.
//
// Tag forms (mirrors PerlinNoiseParity.java):
//   NEW:<seed>:<o0,o1,...>            -> PerlinNoise::create(Xoroshiro(seed), octaves)
//   LBLEND:<seed>:<o0,o1,...>         -> PerlinNoise::createLegacyForBlendedNoise(Legacy(seed), octaves)
//   LNETHER:<seed>:<firstOctave>:<a0hex,a1hex,...>
//                                     -> PerlinNoise::createLegacyForLegacyNetherBiome(Legacy(seed), firstOctave, amps)
//
// Usage: perlin_noise_parity --cases <tsv>

#include "../RandomSource.h"
#include "../Noise.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::PerlinNoise;
using mc::levelgen::XoroshiroRandomSource;

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

static std::vector<std::string> splitOn(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(s);
    while (std::getline(ss, cur, sep)) {
        out.push_back(cur);
    }
    return out;
}

// Build the PerlinNoise described by a construction tag. Returns nullptr on a
// malformed/unsupported tag so the caller can record a mismatch.
static std::unique_ptr<PerlinNoise> buildFromTag(const std::string& tag) {
    // Split into at most-known fields. The first colon-field is the family.
    std::vector<std::string> parts = splitOn(tag, ':');
    if (parts.empty()) return nullptr;
    const std::string& family = parts[0];

    if (family == "NEW") {
        // NEW:<seed>:<o0,o1,...>
        if (parts.size() != 3) return nullptr;
        int64_t seed = static_cast<int64_t>(std::stoll(parts[1]));
        std::vector<int32_t> octaves;
        for (const std::string& o : splitOn(parts[2], ',')) {
            octaves.push_back(static_cast<int32_t>(std::stoi(o)));
        }
        XoroshiroRandomSource rng(seed);
        return std::make_unique<PerlinNoise>(PerlinNoise::create(rng, octaves));
    }

    if (family == "LBLEND") {
        // LBLEND:<seed>:<o0,o1,...>
        if (parts.size() != 3) return nullptr;
        int64_t seed = static_cast<int64_t>(std::stoll(parts[1]));
        std::vector<int32_t> octaves;
        for (const std::string& o : splitOn(parts[2], ',')) {
            octaves.push_back(static_cast<int32_t>(std::stoi(o)));
        }
        LegacyRandomSource rng(seed);
        return std::make_unique<PerlinNoise>(PerlinNoise::createLegacyForBlendedNoise(rng, octaves));
    }

    if (family == "LNETHER") {
        // LNETHER:<seed>:<firstOctave>:<a0hex,a1hex,...>
        if (parts.size() != 4) return nullptr;
        int64_t seed = static_cast<int64_t>(std::stoll(parts[1]));
        int32_t firstOctave = static_cast<int32_t>(std::stoi(parts[2]));
        std::vector<double> amps;
        for (const std::string& a : splitOn(parts[3], ',')) {
            amps.push_back(bd(a));
        }
        LegacyRandomSource rng(seed);
        return std::make_unique<PerlinNoise>(PerlinNoise::createLegacyForLegacyNetherBiome(rng, firstOctave, amps));
    }

    return nullptr;
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
        std::fprintf(stderr, "usage: perlin_noise_parity --cases <tsv>\n");
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
        return 2;
    }

    // Cache one PerlinNoise per construction tag. Building involves running the
    // full RNG init (incl. MD5 hashing for the new-init octaves), so cache it.
    std::map<std::string, std::unique_ptr<PerlinNoise>> cache;
    auto noiseForTag = [&](const std::string& tag) -> PerlinNoise* {
        auto it = cache.find(tag);
        if (it != cache.end()) return it->second.get();
        std::unique_ptr<PerlinNoise> built = buildFromTag(tag);
        PerlinNoise* raw = built.get();
        cache.emplace(tag, std::move(built));
        return raw;
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

        if (tag == "PMAX") {
            // PMAX <ctorTag> <maxValue>
            if (f.size() != 3) { ++mismatches; continue; }
            PerlinNoise* n = noiseForTag(f[1]);
            ++cases;
            if (!n) { ++mismatches; std::fprintf(stderr, "PMAX build failed tag=%s\n", f[1].c_str()); continue; }
            uint64_t expected = std::stoull(f[2], nullptr, 16);
            uint64_t got = db(n->maxValue());
            if (got != expected) {
                ++mismatches;
                std::fprintf(stderr, "PMAX mismatch tag=%s got %016llx exp %016llx\n",
                    f[1].c_str(), (unsigned long long)got, (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "PMBRK") {
            // PMBRK <ctorTag> <yScale> <maxBrokenValue>
            if (f.size() != 4) { ++mismatches; continue; }
            PerlinNoise* n = noiseForTag(f[1]);
            ++cases;
            if (!n) { ++mismatches; std::fprintf(stderr, "PMBRK build failed tag=%s\n", f[1].c_str()); continue; }
            double yScale = bd(f[2]);
            uint64_t expected = std::stoull(f[3], nullptr, 16);
            uint64_t got = db(n->maxBrokenValue(yScale));
            if (got != expected) {
                ++mismatches;
                std::fprintf(stderr, "PMBRK mismatch tag=%s yScale=%016llx got %016llx exp %016llx\n",
                    f[1].c_str(), (unsigned long long)db(yScale),
                    (unsigned long long)got, (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "PVAL") {
            // PVAL <ctorTag> <x> <y> <z> <value>
            if (f.size() != 6) { ++mismatches; continue; }
            PerlinNoise* n = noiseForTag(f[1]);
            ++cases;
            if (!n) { ++mismatches; std::fprintf(stderr, "PVAL build failed tag=%s\n", f[1].c_str()); continue; }
            double x = bd(f[2]), y = bd(f[3]), z = bd(f[4]);
            uint64_t expected = std::stoull(f[5], nullptr, 16);
            uint64_t got = db(n->getValue(x, y, z));
            if (got != expected) {
                ++mismatches;
                std::fprintf(stderr, "PVAL mismatch tag=%s got %016llx exp %016llx\n",
                    f[1].c_str(), (unsigned long long)got, (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "PVALYS") {
            // PVALYS <ctorTag> <x> <y> <z> <yScale> <yFudge> <value>
            if (f.size() != 8) { ++mismatches; continue; }
            PerlinNoise* n = noiseForTag(f[1]);
            ++cases;
            if (!n) { ++mismatches; std::fprintf(stderr, "PVALYS build failed tag=%s\n", f[1].c_str()); continue; }
            double x = bd(f[2]), y = bd(f[3]), z = bd(f[4]);
            double yScale = bd(f[5]), yFudge = bd(f[6]);
            uint64_t expected = std::stoull(f[7], nullptr, 16);
            uint64_t got = db(n->getValue(x, y, z, yScale, yFudge));
            if (got != expected) {
                ++mismatches;
                std::fprintf(stderr, "PVALYS mismatch tag=%s got %016llx exp %016llx\n",
                    f[1].c_str(), (unsigned long long)got, (unsigned long long)expected);
            }
            continue;
        }

        // Unknown tag -> count as mismatch so a malformed TSV cannot pass silently.
        ++mismatches;
        std::fprintf(stderr, "unknown tag '%s'\n", tag.c_str());
    }

    std::printf("PerlinNoise cases=%lld mismatches=%lld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
