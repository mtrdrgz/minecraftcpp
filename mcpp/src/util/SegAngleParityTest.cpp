// Parity test for net.minecraft.util.SegmentedAnglePrecision. Ground truth:
// tools/SegAngleParity.java vs the real class. Recomputes every method with the C++
// port and compares bit-for-bit (floats as raw IEEE-754 bits via std::bit_cast).
//
//   seg_angle_parity --cases mcpp/build/seg_angle.tsv

#include "SegmentedAnglePrecision.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace seg = mc::util;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int      i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
uint32_t hx(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: seg_angle_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqF = [&](float got, const std::string& exp, const std::string& l) {
        if (fb(got) != hx(exp)) fail(l + " gotbits=" + std::to_string(fb(got)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "FIELDS") {
            // p mask precision degreeToAngle angleToDegree getMask
            seg::SegmentedAnglePrecision s(i(p[1]));
            eqI(s.getMask(), p[2], line);
            eqI(s.getPrecision(), p[3], line);
            eqF(s.getDegreeToAngle(), p[4], line);
            eqF(s.getAngleToDegree(), p[5], line);
            eqI(s.getMask(), p[6], line);
        } else if (t == "FROMDIR") {
            // p data2d isVertical out
            seg::SegmentedAnglePrecision s(i(p[1]));
            int got = s.fromDirection(i(p[2]), i(p[3]) != 0);
            eqI(got, p[4], line);
        } else if (t == "NORM") {
            // p ba normalize toDegreesWithTurns toDegrees
            seg::SegmentedAnglePrecision s(i(p[1]));
            int ba = i(p[2]);
            eqI(s.normalize(ba), p[3], line);
            eqF(s.toDegreesWithTurns(ba), p[4], line);
            eqF(s.toDegrees(ba), p[5], line);
        } else if (t == "SAMEAXIS") {
            // p a b same
            seg::SegmentedAnglePrecision s(i(p[1]));
            int got = s.isSameAxis(i(p[2]), i(p[3])) ? 1 : 0;
            eqI(got, p[4], line);
        } else if (t == "FROMDEG") {
            // p degrees(bits) fromDegreesWithTurns fromDegrees
            seg::SegmentedAnglePrecision s(i(p[1]));
            float deg = std::bit_cast<float>(hx(p[2]));
            eqI(s.fromDegreesWithTurns(deg), p[3], line);
            eqI(s.fromDegrees(deg), p[4], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SegmentedAnglePrecision cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
