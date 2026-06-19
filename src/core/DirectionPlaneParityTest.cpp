// Bit-exact parity gate (verify-existing) for the requested subset of
// net.minecraft.core.Direction (Minecraft 26.1.2):
//   from2DDataValue, get2DDataValue, toYRot, fromYRot, getAxisDirection,
//   getOpposite, and Direction.Plane HORIZONTAL/VERTICAL iteration order.
//
// These were ALREADY ported & certified; this gate re-verifies the existing
// headers (no new header is written, no existing file edited):
//   core/Direction2D.h        -> 2D-data / yRot / opposite (namespace mc::)
//   core/DirectionAxisExtra.h -> getAxisDirection + Plane iteration (namespace mc::core_dirextra::)
//
// Reads the TSV emitted by mcpp/tools/DirectionPlaneParity.java and compares
// BIT-FOR-BIT (floats as raw IEEE-754 bits via std::bit_cast).
//
//   direction_plane_parity --cases mcpp/build/direction_plane.tsv

#include "core/Direction2D.h"        // mc::Direction, direction2D* helpers, directionOpposite
#include "core/DirectionAxisExtra.h" // mc::core_dirextra:: Plane / Axis / AxisDirection helpers

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dx = mc::core_dirextra;

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
static uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
// INT_MIN/INT_MAX-safe decimal parse.
static int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a C++ ordinal (DOWN=0..EAST=5) to mc::Direction (from world/phys/Direction.h).
static mc::Direction dirFromOrd(int o) { return mc::DIRECTION_VALUES[o]; }

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: direction_plane_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "GET2D") {
            // <ord> <expected get2DDataValue>
            mc::Direction d = dirFromOrd(i32(p[1]));
            int got = mc::direction2DGet2DDataValue(d);
            if (got != i32(p[2])) fail(line + " got=" + std::to_string(got));
        } else if (tag == "TOYROT") {
            // <ord> <toYRot bits>
            mc::Direction d = dirFromOrd(i32(p[1]));
            uint32_t got = fb(mc::direction2DToYRot(d));
            if (got != fb(bf(p[2]))) fail(line + " gotbits=" + std::to_string(got));
        } else if (tag == "AXDIR") {
            // <ord> <getAxisDirection ord> <getAxis ord> <getOpposite ord>
            int o = i32(p[1]);
            dx::Direction de = static_cast<dx::Direction>(o);
            mc::Direction dm = dirFromOrd(o);
            if (static_cast<int>(dx::directionAxisDirection(de)) != i32(p[2]))
                fail(line + " axisdir=" + std::to_string(static_cast<int>(dx::directionAxisDirection(de))));
            if (static_cast<int>(dx::directionAxis(de)) != i32(p[3]))
                fail(line + " axis=" + std::to_string(static_cast<int>(dx::directionAxis(de))));
            if (static_cast<int>(mc::direction2DGetOpposite(dm)) != i32(p[4]))
                fail(line + " opp=" + std::to_string(static_cast<int>(mc::direction2DGetOpposite(dm))));
        } else if (tag == "FROM2D") {
            // <data> <expected ord>
            int got = static_cast<int>(mc::direction2DFrom2DDataValue(i32(p[1])));
            if (got != i32(p[2])) fail(line + " got=" + std::to_string(got));
        } else if (tag == "FROMYROT") {
            // <yRot bits> <expected ord>
            int got = static_cast<int>(mc::direction2DFromYRot(bd(p[1])));
            if (got != i32(p[2])) fail(line + " got=" + std::to_string(got));
        } else if (tag == "PLEN") {
            // <planeOrd> <length>
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            if (dx::planeLength(pl) != i32(p[2])) fail(line + " len=" + std::to_string(dx::planeLength(pl)));
        } else if (tag == "PFACE") {
            // <planeOrd> <index> <faceOrd>
            dx::Plane pl = static_cast<dx::Plane>(i32(p[1]));
            int got = static_cast<int>(dx::planeFace(pl, i32(p[2])));
            if (got != i32(p[3])) fail(line + " face=" + std::to_string(got));
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "DirectionPlane cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
