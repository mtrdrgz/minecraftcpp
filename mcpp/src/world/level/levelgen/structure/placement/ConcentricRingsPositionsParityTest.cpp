// Bit-exact parity gate for the concentric-rings (stronghold) ring-position
// skeleton: ConcentricRingsPositions.h vs the REAL 26.1.2
// ChunkGeneratorStructureState.generateRingPositions (driven, with the biome search
// stubbed to null, by tools/ConcentricRingsPositionsParity.java).
//
// Usage:  concentric_rings_positions_test --cases <tsv>
// Prints: ConcentricRingsPositions checks=N mismatches=M    (exit nonzero iff M>0)
//
// TSV rows:
//   PARAMS  <seed>  <distance>  <count>  <spread>
//   POS     <seed>  <i>  <chunkX>  <chunkZ>

#include "ConcentricRingsPositions.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

using mc::levelgen::structure::placement::RingChunkPos;
using mc::levelgen::structure::placement::generateRingPositionsSkeleton;

struct Params {
    int32_t distance{};
    int32_t count{};
    int32_t spread{};
    bool seen{false};
};

}  // namespace

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (!casesPath) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open cases file: %s\n", casesPath);
        return 2;
    }

    // Per-seed params and expected positions.
    std::map<int64_t, Params> params;
    std::map<int64_t, std::vector<RingChunkPos>> expected;  // index i -> (x,z)
    std::map<int64_t, std::vector<bool>> haveExpected;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string tag;
        std::getline(ss, tag, '\t');
        if (tag == "PARAMS") {
            int64_t seed = 0;
            int32_t distance = 0, count = 0, spread = 0;
            std::string f;
            std::getline(ss, f, '\t'); seed = std::stoll(f);
            std::getline(ss, f, '\t'); distance = static_cast<int32_t>(std::stol(f));
            std::getline(ss, f, '\t'); count = static_cast<int32_t>(std::stol(f));
            std::getline(ss, f, '\t'); spread = static_cast<int32_t>(std::stol(f));
            params[seed] = Params{distance, count, spread, true};
        } else if (tag == "POS") {
            int64_t seed = 0;
            int32_t idx = 0, x = 0, z = 0;
            std::string f;
            std::getline(ss, f, '\t'); seed = std::stoll(f);
            std::getline(ss, f, '\t'); idx = static_cast<int32_t>(std::stol(f));
            std::getline(ss, f, '\t'); x = static_cast<int32_t>(std::stol(f));
            std::getline(ss, f, '\t'); z = static_cast<int32_t>(std::stol(f));
            auto& vec = expected[seed];
            auto& have = haveExpected[seed];
            if (static_cast<std::size_t>(idx) >= vec.size()) {
                vec.resize(static_cast<std::size_t>(idx) + 1);
                have.resize(static_cast<std::size_t>(idx) + 1, false);
            }
            vec[static_cast<std::size_t>(idx)] = RingChunkPos{x, z};
            have[static_cast<std::size_t>(idx)] = true;
        }
    }

    long long checks = 0;
    long long mismatches = 0;
    int shown = 0;

    for (const auto& [seed, p] : params) {
        if (!p.seen) {
            continue;
        }
        std::vector<RingChunkPos> got = generateRingPositionsSkeleton(seed, p.distance, p.count, p.spread);
        const auto& exp = expected[seed];
        const auto& have = haveExpected[seed];

        if (static_cast<int32_t>(got.size()) != p.count) {
            ++mismatches;
            if (shown++ < 20) {
                std::fprintf(stderr, "seed=%lld size mismatch: got=%zu count=%d\n",
                             static_cast<long long>(seed), got.size(), p.count);
            }
            continue;
        }

        for (std::size_t i = 0; i < got.size(); i++) {
            if (i >= exp.size() || !have[i]) {
                ++mismatches;
                if (shown++ < 20) {
                    std::fprintf(stderr, "seed=%lld i=%zu missing expected row\n",
                                 static_cast<long long>(seed), i);
                }
                continue;
            }
            ++checks;
            if (!(got[i] == exp[i])) {
                ++mismatches;
                if (shown++ < 20) {
                    std::fprintf(stderr,
                                 "seed=%lld i=%zu got=(%d,%d) exp=(%d,%d)\n",
                                 static_cast<long long>(seed), i,
                                 got[i].x, got[i].z, exp[i].x, exp[i].z);
                }
            }
        }
    }

    std::printf("ConcentricRingsPositions checks=%lld mismatches=%lld\n", checks, mismatches);
    return mismatches == 0 ? 0 : 1;
}
