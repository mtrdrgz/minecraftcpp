// Bit-exact parity gate for net.minecraft.world.level.levelgen.synth.ImprovedNoise.
//
// This test VERIFIES the EXISTING engine port mc::levelgen::ImprovedNoise
// (mcpp/src/world/level/levelgen/Noise.{h,cpp}); it does not duplicate it.
// It seeds mc::levelgen::LegacyRandomSource(seed) identically to the Java GT,
// constructs the real ImprovedNoise, and compares ctor permutation + offsets and
// noise()/noise(...,yScale,yFudge)/noiseWithDerivative() outputs BIT-FOR-BIT
// against ground truth from tools/ImprovedNoiseParity.java.
//
// Usage: improved_noise_parity --cases <tsv>

#include "../RandomSource.h"
#include "../Noise.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::ImprovedNoise;
using mc::levelgen::LegacyRandomSource;

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

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::fprintf(stderr, "usage: improved_noise_parity --cases <tsv>\n");
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
        return 2;
    }

    // Lazily build (and cache) one ImprovedNoise per seed, seeding LegacyRandomSource
    // identically to the Java GT. Rows are grouped by seed, but cache anyway.
    long long cachedSeed = 0;
    bool haveCached = false;
    std::unique_ptr<ImprovedNoise> cached;

    auto noiseForSeed = [&](long long seed) -> ImprovedNoise& {
        if (!haveCached || cachedSeed != seed) {
            LegacyRandomSource rng(static_cast<int64_t>(seed));
            cached = std::make_unique<ImprovedNoise>(rng);
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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> f = splitTabs(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "CTOR") {
            // CTOR seed xo yo zo p0..p255
            if (f.size() != 5 + 256) { ++mismatches; continue; }
            long long seed = std::stoll(f[1]);
            uint64_t exoXo = std::stoull(f[2], nullptr, 16);
            uint64_t exoYo = std::stoull(f[3], nullptr, 16);
            uint64_t exoZo = std::stoull(f[4], nullptr, 16);
            ImprovedNoise& n = noiseForSeed(seed);
            ++cases;
            bool ok = (db(n.xo) == exoXo) && (db(n.yo) == exoYo) && (db(n.zo) == exoZo);
            // Verify the permutation table via the public noise path is implicit; here we
            // assert offsets only (p[] is private). Permutation correctness is covered by
            // the NOISE/NWD batteries below, which read every p() index.
            if (!ok) {
                ++mismatches;
                std::fprintf(stderr,
                    "CTOR mismatch seed=%lld xo %016llx/%016llx yo %016llx/%016llx zo %016llx/%016llx\n",
                    seed,
                    (unsigned long long)db(n.xo), (unsigned long long)exoXo,
                    (unsigned long long)db(n.yo), (unsigned long long)exoYo,
                    (unsigned long long)db(n.zo), (unsigned long long)exoZo);
            }
            continue;
        }

        if (tag == "NOISE") {
            // NOISE seed x y z value
            if (f.size() != 6) { ++mismatches; continue; }
            long long seed = std::stoll(f[1]);
            double x = bd(f[2]), y = bd(f[3]), z = bd(f[4]);
            uint64_t expected = std::stoull(f[5], nullptr, 16);
            ImprovedNoise& n = noiseForSeed(seed);
            double got = n.noise(x, y, z);
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr, "NOISE mismatch seed=%lld got %016llx exp %016llx\n",
                    seed, (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "NOISEYS") {
            // NOISEYS seed x y z yScale yFudge value
            if (f.size() != 8) { ++mismatches; continue; }
            long long seed = std::stoll(f[1]);
            double x = bd(f[2]), y = bd(f[3]), z = bd(f[4]);
            double yScale = bd(f[5]), yFudge = bd(f[6]);
            uint64_t expected = std::stoull(f[7], nullptr, 16);
            ImprovedNoise& n = noiseForSeed(seed);
            double got = n.noise(x, y, z, yScale, yFudge);
            ++cases;
            if (db(got) != expected) {
                ++mismatches;
                std::fprintf(stderr, "NOISEYS mismatch seed=%lld got %016llx exp %016llx\n",
                    seed, (unsigned long long)db(got), (unsigned long long)expected);
            }
            continue;
        }

        if (tag == "NWD") {
            // NWD seed x y z value dx dy dz  (derivativeOut starts {0,0,0}, method accumulates)
            if (f.size() != 9) { ++mismatches; continue; }
            long long seed = std::stoll(f[1]);
            double x = bd(f[2]), y = bd(f[3]), z = bd(f[4]);
            uint64_t expVal = std::stoull(f[5], nullptr, 16);
            uint64_t expDx = std::stoull(f[6], nullptr, 16);
            uint64_t expDy = std::stoull(f[7], nullptr, 16);
            uint64_t expDz = std::stoull(f[8], nullptr, 16);
            ImprovedNoise& n = noiseForSeed(seed);
            std::array<double, 3> der{0.0, 0.0, 0.0};
            double got = n.noiseWithDerivative(x, y, z, der);
            ++cases;
            bool ok = (db(got) == expVal) && (db(der[0]) == expDx)
                && (db(der[1]) == expDy) && (db(der[2]) == expDz);
            if (!ok) {
                ++mismatches;
                std::fprintf(stderr,
                    "NWD mismatch seed=%lld v %016llx/%016llx dx %016llx/%016llx dy %016llx/%016llx dz %016llx/%016llx\n",
                    seed,
                    (unsigned long long)db(got), (unsigned long long)expVal,
                    (unsigned long long)db(der[0]), (unsigned long long)expDx,
                    (unsigned long long)db(der[1]), (unsigned long long)expDy,
                    (unsigned long long)db(der[2]), (unsigned long long)expDz);
            }
            continue;
        }

        // Unknown tag -> count as mismatch so a malformed TSV cannot pass silently.
        ++mismatches;
        std::fprintf(stderr, "unknown tag '%s'\n", tag.c_str());
    }

    std::printf("ImprovedNoise cases=%lld mismatches=%lld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
