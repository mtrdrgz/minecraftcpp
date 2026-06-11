// Parity test for the COMPLETE boolean predicate
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest.test()
//
// Ground truth: tools/AxisAlignedLinearPosTestPredicateParity.java drives the REAL
// rule's test() over two certified RandomSource flavours (Legacy + Xoroshiro),
// seeded identically, calling test() N consecutive times per row. We construct the
// SAME RandomSource on the C++ side, drive the ported AxisAlignedLinearPosTest, and
// compare:
//   * the integer distance (exact decimal),
//   * the float threshold (Float.floatToRawIntBits, compared bit-exact),
//   * each of the N boolean test() results (0/1) — the actual predicate decision,
//     which also proves the nextFloat() stream advanced identically.
//
//   axis_aligned_linear_pos_predicate_parity \
//       --cases mcpp/build/axis_aligned_linear_pos_predicate.tsv

#include "AxisAlignedLinearPosTestPredicate.h"

#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::templatesystem;
using mc::levelgen::RandomSource;
using mc::levelgen::LegacyRandomSource;
using mc::levelgen::XoroshiroRandomSource;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }
long long tol(const std::string& s) { return std::stoll(s); }

// Float.floatToRawIntBits (decimal int) -> float, zero rounding.
float fromBits(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(static_cast<int32_t>(std::stoll(s))));
}

// Build the C++ RandomSource matching the Java flavour, seeded identically.
std::unique_ptr<RandomSource> makeRng(const std::string& rng, int64_t seed) {
    if (rng == "LEG") return std::make_unique<LegacyRandomSource>(seed);
    if (rng == "XOR") return std::make_unique<XoroshiroRandomSource>(seed);
    return nullptr;
}

constexpr int N = 8; // consecutive test() calls per row (must match the Java N)

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: axis_aligned_linear_pos_predicate_parity --cases <tsv>\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        if (p.empty() || p[0] != "AALP") continue; // skip JVM bootstrap log lines
        ++total;

        // AALP rng seed axisOrd wx wy wz rx ry rz minDist maxDist minChanceBits maxChanceBits distInt thrBits r0..r{N-1}
        const std::string& rng = p[1];
        int64_t seed = tol(p[2]);
        Axis axis = static_cast<Axis>(toi(p[3]));
        int wx = toi(p[4]), wy = toi(p[5]), wz = toi(p[6]);
        int rx = toi(p[7]), ry = toi(p[8]), rz = toi(p[9]);
        int minDist = toi(p[10]), maxDist = toi(p[11]);
        float minChance = fromBits(p[12]), maxChance = fromBits(p[13]);
        int expDist = toi(p[14]);
        float expThr = fromBits(p[15]);

        // Distance + threshold cross-check against the diagnostics columns.
        int gotDist = axisAlignedDist(axis, wx, wy, wz, rx, ry, rz);
        float gotThr = acceptanceThreshold(gotDist, minDist, maxDist, minChance, maxChance);
        if (gotDist != expDist ||
            std::bit_cast<uint32_t>(gotThr) != std::bit_cast<uint32_t>(expThr)) {
            fail(line + " gotDist=" + std::to_string(gotDist) +
                 " gotThrBits=" + std::to_string(std::bit_cast<int32_t>(gotThr)));
            continue;
        }

        AxisAlignedLinearPosTest rule(minChance, maxChance, minDist, maxDist, axis);
        auto random = makeRng(rng, seed);
        if (!random) { fail(line + " unknown rng " + rng); continue; }

        bool rowOk = true;
        for (int k = 0; k < N; ++k) {
            bool got = rule.test(wx, wy, wz, rx, ry, rz, *random);
            int exp = toi(p[16 + k]);
            if ((got ? 1 : 0) != exp) {
                rowOk = false;
                fail(line + " call=" + std::to_string(k) +
                     " got=" + std::to_string(got ? 1 : 0) + " exp=" + std::to_string(exp));
                break;
            }
        }
        (void)rowOk;
    }

    std::cout << "AxisAlignedLinearPosTestPredicate checks=" << total
              << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
