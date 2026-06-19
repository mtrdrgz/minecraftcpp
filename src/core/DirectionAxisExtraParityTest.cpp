// Parity test for the still-unported "extra" surface of net.minecraft.core.Direction
// (MC 26.1.2). Ground truth: tools/DirectionAxisExtraParity.java vs the real class.
// Recomputes every method with the C++ port (core/DirectionAxisExtra.h) and compares
// bit-for-bit (doubles as raw IEEE-754 bits via std::bit_cast).
//
//   direction_axis_parity --cases mcpp/build/direction_axis.tsv

#include "DirectionAxisExtra.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dx = mc::core_dirextra;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Parse a possibly-INT_MIN/INT_MAX decimal int safely via 64-bit then cast.
int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }
uint64_t hx64(const std::string& s) { return static_cast<uint64_t>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: direction_axis_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqD = [&](double got, const std::string& exp, const std::string& l) {
        if (db(got) != hx64(exp)) fail(l + " gotbits=" + std::to_string(db(got)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ICHOOSE") {
            // axisOrd x y z expected
            dx::Axis ax = static_cast<dx::Axis>(i32(p[1]));
            int got = dx::axisChoose<int>(ax, i32(p[2]), i32(p[3]), i32(p[4]));
            eqI(got, p[5], line);
        } else if (t == "DCHOOSE") {
            // axisOrd xbits ybits zbits expectedbits
            dx::Axis ax = static_cast<dx::Axis>(i32(p[1]));
            double x = std::bit_cast<double>(hx64(p[2]));
            double y = std::bit_cast<double>(hx64(p[3]));
            double z = std::bit_cast<double>(hx64(p[4]));
            double got = dx::axisChoose<double>(ax, x, y, z);
            eqD(got, p[5], line);
        } else if (t == "AXFLAG") {
            // axisOrd isHoriz isVert getPositive getNegative getPlane
            dx::Axis ax = static_cast<dx::Axis>(i32(p[1]));
            eqI(dx::axisIsHorizontal(ax) ? 1 : 0, p[2], line);
            eqI(dx::axisIsVertical(ax) ? 1 : 0, p[3], line);
            eqI(static_cast<int>(dx::axisGetPositive(ax)), p[4], line);
            eqI(static_cast<int>(dx::axisGetNegative(ax)), p[5], line);
            eqI(static_cast<int>(dx::axisGetPlane(ax)), p[6], line);
        } else if (t == "AXDIR") {
            // dirOrd getAxis getAxisDirection
            dx::Direction d = static_cast<dx::Direction>(i32(p[1]));
            eqI(static_cast<int>(dx::directionAxis(d)), p[2], line);
            eqI(static_cast<int>(dx::directionAxisDirection(d)), p[3], line);
        } else if (t == "PLANE") {
            // planeOrd length axisCount
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            eqI(dx::planeLength(pl), p[2], line);
            eqI(dx::planeAxisCount(pl), p[3], line);
        } else if (t == "PFACE") {
            // planeOrd index faceOrd
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            int idx = i32(p[2]);
            eqI(static_cast<int>(dx::planeFace(pl, idx)), p[3], line);
        } else if (t == "PAXIS") {
            // planeOrd index axisOrd
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            int idx = i32(p[2]);
            eqI(static_cast<int>(dx::planeAxis(pl, idx)), p[3], line);
        } else if (t == "PTEST") {
            // planeOrd dirOrd test
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            dx::Direction d = static_cast<dx::Direction>(i32(p[2]));
            eqI(dx::planeTest(pl, d) ? 1 : 0, p[3], line);
        } else if (t == "NEAREST") {
            // x y z orElseOrd resultOrd
            int got = dx::directionGetNearest(i32(p[1]), i32(p[2]), i32(p[3]), i32(p[4]));
            eqI(got, p[5], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "DirectionAxisExtra cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
