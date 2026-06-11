// Parity test for the PURE, RNG-driven static helpers of
//   net.minecraft.world.level.levelgen.structure.structures.RuinedPortalStructure
//     .sample(WorldgenRandom, float)                       (26.1.2)
//     .getRandomWithinInterval(RandomSource, int, int)     (26.1.2)
//
// Ground truth: tools/RuinedPortalYSelectorParity.java drives the REAL private
// static methods via reflection over a REAL seeded RandomSource and emits a TSV.
// We recompute each row from RuinedPortalYSelector.h (replaying the SAME seed with
// the certified mc::levelgen::LegacyRandomSource) and compare. sample() returns a
// bool and getRandomWithinInterval() returns an int, so both gates are exact.
//
//   ruined_portal_yselector_parity --cases mcpp/build/ruined_portal_yselector.tsv
//
// Any row whose first tab-field is not a known tag (SAMPLE / INTERVAL) is SKIPPED
// (not counted, not a mismatch), so stray Bootstrap stdout can't corrupt counts.

#include "RuinedPortalYSelector.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "world/level/levelgen/RandomSource.h"

using namespace mc::levelgen::structure::ruinedportal;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int32_t toi(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
int64_t tol(const std::string& s) { return static_cast<int64_t>(std::stoll(s)); }

float floatFromBits(const std::string& s) {
    uint32_t bits = static_cast<uint32_t>(std::stoul(s));
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: ruined_portal_yselector_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "SAMPLE") {
            // limitBits seed | result(0|1)
            if (p.size() != 4) continue;  // malformed/partial: skip, don't count
            float limit = floatFromBits(p[1]);
            int64_t seed = tol(p[2]);
            int expected = toi(p[3]);
            mc::levelgen::LegacyRandomSource rng(seed);
            bool got = sample(rng, limit);
            ++total;
            if ((got ? 1 : 0) != expected)
                fail(line + " got=" + std::to_string(got ? 1 : 0));
        } else if (tag == "INTERVAL") {
            // minPreferred max seed | result
            if (p.size() != 5) continue;  // malformed/partial: skip, don't count
            int32_t minPreferred = toi(p[1]);
            int32_t max = toi(p[2]);
            int64_t seed = tol(p[3]);
            int32_t expected = toi(p[4]);
            mc::levelgen::LegacyRandomSource rng(seed);
            int32_t got = getRandomWithinInterval(rng, minPreferred, max);
            ++total;
            if (got != expected)
                fail(line + " got=" + std::to_string(got));
        }
        // Unknown tags (Bootstrap chatter) are silently skipped.
    }

    std::cout << "RuinedPortalYSelector checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
