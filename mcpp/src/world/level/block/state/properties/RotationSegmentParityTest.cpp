// Parity test for
// net.minecraft.world.level.block.state.properties.RotationSegment. Ground truth:
// tools/RotationSegmentParity.java vs the real class. Recomputes every public static
// method with the C++ port and compares bit-for-bit (floats as raw IEEE-754 bits via
// std::bit_cast).
//
//   rotation_segment_parity --cases mcpp/build/rotation_segment.tsv

#include "RotationSegment.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace rs = mc::world::level::block::state::properties;

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
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: rotation_segment_parity --cases <tsv>\n"; return 2; }
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

        if (t == "MAXIDX") {
            // getMaxSegmentIndex()
            eqI(rs::RotationSegment::getMaxSegmentIndex(), p[1], line);
        } else if (t == "FROMDIR") {
            // data2d isVertical convertToSegment
            int got = rs::RotationSegment::convertToSegment(i(p[1]), i(p[2]) != 0);
            eqI(got, p[3], line);
        } else if (t == "FROMDEG") {
            // degrees(bits) convertToSegment
            float deg = std::bit_cast<float>(hx(p[1]));
            eqI(rs::RotationSegment::convertToSegment(deg), p[2], line);
        } else if (t == "TODEG") {
            // segment convertToDegrees(bits) convertToDirection(data3d|-1)
            int seg = i(p[1]);
            eqF(rs::RotationSegment::convertToDegrees(seg), p[2], line);
            eqI(rs::RotationSegment::convertToDirection(seg), p[3], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "RotationSegment cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
