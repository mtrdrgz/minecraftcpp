// Parity test for net.minecraft.world.level.portal.PortalShape.getRelativePosition.
// Ground truth: tools/PortalShapeParity.java (drives the REAL class reflectively).
// Bit-exact: every double is compared by its raw IEEE-754 bits (doubleToRawLongBits
// vs std::bit_cast), so NaN / -0.0 / Inf / saturated clamps are checked faithfully.
//
//   portal_shape_parity --cases mcpp/build/portal_shape.tsv

#include "world/level/portal/PortalShape.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::portal::FoundRectangle;
using mc::portal::getRelativePosition;
using mc::Vec3;
using mc::Axis;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// 16-hex -> double bits; 8-hex -> float bits.
double   bd(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16))); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint64_t hx(const std::string& s) { return static_cast<uint64_t>(std::stoull(s, nullptr, 16)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: portal_shape_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        ++total;

        if (p[0] != "REL" || p.size() < 15) { fail("UNKNOWN_OR_SHORT " + line); continue; }

        FoundRectangle rect;
        int axisOrd      = std::stoi(p[1]);
        rect.minCornerX  = std::stoi(p[2]);
        rect.minCornerY  = std::stoi(p[3]);
        rect.minCornerZ  = std::stoi(p[4]);
        rect.axis1Size   = std::stoi(p[5]);
        rect.axis2Size   = std::stoi(p[6]);
        Vec3 pos{bd(p[7]), bd(p[8]), bd(p[9])};
        float width  = bf(p[10]);
        float height = bf(p[11]);

        Vec3 got = getRelativePosition(rect, static_cast<Axis>(axisOrd), pos, width, height);

        if (db(got.x) != hx(p[12]) || db(got.y) != hx(p[13]) || db(got.z) != hx(p[14]))
            fail(line);
    }

    std::cout << "PortalShape checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
