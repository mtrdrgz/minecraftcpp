// Parity test for the PURE distance + acceptance-threshold math in
//   net.minecraft.world.level.levelgen.structure.templatesystem.LinearPosTest
//   net.minecraft.world.level.levelgen.structure.templatesystem.AxisAlignedLinearPosTest
//
// Ground truth: tools/LinearPosTestMathParity.java drives the REAL classes'
// private fields (reflection) and the REAL net.minecraft.util.Mth to emit a TSV.
// We recompute each row from LinearPosTestMath.h and compare:
//   * distances are decimal ints (exact),
//   * the float threshold is exchanged as Float.floatToRawIntBits and compared
//     bit-exact via std::bit_cast<float> (no tolerance — a mismatch is a port bug).
//
//   linear_pos_test_math_parity --cases mcpp/build/linear_pos_test_math.tsv

#include "LinearPosTestMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen::structure::templatesystem;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return static_cast<int>(std::stoll(s)); }

// Raw int bits (Float.floatToRawIntBits) -> float, with zero rounding.
float fromBits(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(static_cast<int32_t>(std::stoll(s))));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: linear_pos_test_math_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << tag << " " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTabs(line);
        const std::string& tag = p[0];

        if (tag == "LIN") {
            // wx wy wz rx ry rz minDist maxDist minChanceBits maxChanceBits | distInt thrBits
            ++total;
            int wx = toi(p[1]), wy = toi(p[2]), wz = toi(p[3]);
            int rx = toi(p[4]), ry = toi(p[5]), rz = toi(p[6]);
            int minDist = toi(p[7]), maxDist = toi(p[8]);
            float minChance = fromBits(p[9]), maxChance = fromBits(p[10]);
            int expDist = toi(p[11]);
            float expThr = fromBits(p[12]);

            int gotDist = distManhattan(wx, wy, wz, rx, ry, rz);
            float gotThr = acceptanceThreshold(gotDist, minDist, maxDist, minChance, maxChance);

            if (gotDist != expDist ||
                std::bit_cast<uint32_t>(gotThr) != std::bit_cast<uint32_t>(expThr)) {
                fail(tag, line + " gotDist=" + std::to_string(gotDist) +
                          " gotThrBits=" + std::to_string(std::bit_cast<int32_t>(gotThr)));
            }
        } else if (tag == "AXIS") {
            // axisOrd wx wy wz rx ry rz minDist maxDist minChanceBits maxChanceBits | distInt thrBits
            ++total;
            Axis axis = static_cast<Axis>(toi(p[1]));
            int wx = toi(p[2]), wy = toi(p[3]), wz = toi(p[4]);
            int rx = toi(p[5]), ry = toi(p[6]), rz = toi(p[7]);
            int minDist = toi(p[8]), maxDist = toi(p[9]);
            float minChance = fromBits(p[10]), maxChance = fromBits(p[11]);
            int expDist = toi(p[12]);
            float expThr = fromBits(p[13]);

            int gotDist = axisAlignedDist(axis, wx, wy, wz, rx, ry, rz);
            float gotThr = acceptanceThreshold(gotDist, minDist, maxDist, minChance, maxChance);

            if (gotDist != expDist ||
                std::bit_cast<uint32_t>(gotThr) != std::bit_cast<uint32_t>(expThr)) {
                fail(tag, line + " gotDist=" + std::to_string(gotDist) +
                          " gotThrBits=" + std::to_string(std::bit_cast<int32_t>(gotThr)));
            }
        } else {
            // Unknown leading field (e.g. JVM bootstrap log lines) — skip silently.
            continue;
        }
    }

    std::cout << "LinearPosTestMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
