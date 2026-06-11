// Parity test for the PURE math primitives of net.minecraft.world.level.biome.Climate.
// Ground truth: tools/ClimateMathParity.java drives the REAL decompiled Climate class;
// this test reads the TSV and recomputes each value with the C++ port (ClimateMath.h),
// comparing exactly: floats via raw IEEE bits (bit_cast), longs in decimal.
//
//   climate_math_parity --cases mcpp/build/climate_math.tsv
//
// Row formats (see ClimateMathParity.java):
//   Q    <coordBits>                          <long quantizeCoord>
//   U    <long coord>                          <float bits unquantizeCoord>
//   T    <6 float bits>                         <6 longs>
//   PT   <minBits> <maxBits>                    <pMin> <pMax>
//   PD   <pMin> <pMax> <targetLong>             <long distance(long)>
//   PDP  <aMin> <aMax> <bMin> <bMax>            <long distance(Parameter)>
//   FIT  <12 longs (param mins/maxs)> <offset>  <6 target longs>  <long fitness>

#include "ClimateMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cm = mc::biome::ClimateMath;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Parse an 8-hex raw-IEEE-bits token into a float (matches Java floatToRawIntBits).
float floatFromBits(const std::string& hex) {
    const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    return std::bit_cast<float>(bits);
}

uint32_t bitsOf(float v) { return std::bit_cast<uint32_t>(v); }

int64_t parseL(const std::string& s) {
    // std::stoll handles full int64 range including Long.MIN/MAX.
    return static_cast<int64_t>(std::stoll(s));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: climate_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "Q") {
            // Q  coordBits  expectedLong
            const float coord = floatFromBits(p[1]);
            const int64_t exp = parseL(p[2]);
            const int64_t got = cm::quantizeCoord(coord);
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else if (t == "U") {
            // U  longCoord  expectedFloatBits
            const int64_t coord = parseL(p[1]);
            const uint32_t exp = static_cast<uint32_t>(std::stoul(p[2], nullptr, 16));
            const uint32_t got = bitsOf(cm::unquantizeCoord(coord));
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else if (t == "T") {
            // T  tBits hBits cBits eBits dBits wBits  t h c e d w (6 longs)
            const cm::TargetPoint tp = cm::target(
                floatFromBits(p[1]), floatFromBits(p[2]), floatFromBits(p[3]),
                floatFromBits(p[4]), floatFromBits(p[5]), floatFromBits(p[6]));
            const int64_t e0 = parseL(p[7]), e1 = parseL(p[8]), e2 = parseL(p[9]),
                          e3 = parseL(p[10]), e4 = parseL(p[11]), e5 = parseL(p[12]);
            if (tp.temperature != e0 || tp.humidity != e1 || tp.continentalness != e2 ||
                tp.erosion != e3 || tp.depth != e4 || tp.weirdness != e5) {
                fail(line + " got=" + std::to_string(tp.temperature) + "," + std::to_string(tp.humidity) + "," +
                     std::to_string(tp.continentalness) + "," + std::to_string(tp.erosion) + "," +
                     std::to_string(tp.depth) + "," + std::to_string(tp.weirdness));
            }
        } else if (t == "PT") {
            // PT  minBits maxBits  pMin pMax
            const float mn = floatFromBits(p[1]);
            const float mx = floatFromBits(p[2]);
            const int64_t eMin = parseL(p[3]), eMax = parseL(p[4]);
            const cm::Parameter par = cm::Parameter::span(mn, mx);
            if (par.min != eMin || par.max != eMax)
                fail(line + " got=" + std::to_string(par.min) + "," + std::to_string(par.max));
        } else if (t == "PD") {
            // PD  pMin pMax targetLong  distance
            const cm::Parameter par{ parseL(p[1]), parseL(p[2]) };
            const int64_t target = parseL(p[3]);
            const int64_t exp = parseL(p[4]);
            const int64_t got = par.distance(target);
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else if (t == "PDP") {
            // PDP  aMin aMax bMin bMax  distance
            const cm::Parameter a{ parseL(p[1]), parseL(p[2]) };
            const cm::Parameter b{ parseL(p[3]), parseL(p[4]) };
            const int64_t exp = parseL(p[5]);
            const int64_t got = a.distance(b);
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else if (t == "FIT") {
            // FIT  tMin tMax hMin hMax cMin cMax eMin eMax dMin dMax wMin wMax offset
            //      tt th tc te td tw  fitness
            cm::ParameterPoint pp{
                cm::Parameter{ parseL(p[1]),  parseL(p[2]) },
                cm::Parameter{ parseL(p[3]),  parseL(p[4]) },
                cm::Parameter{ parseL(p[5]),  parseL(p[6]) },
                cm::Parameter{ parseL(p[7]),  parseL(p[8]) },
                cm::Parameter{ parseL(p[9]),  parseL(p[10]) },
                cm::Parameter{ parseL(p[11]), parseL(p[12]) },
                parseL(p[13])
            };
            cm::TargetPoint tp{ parseL(p[14]), parseL(p[15]), parseL(p[16]),
                                parseL(p[17]), parseL(p[18]), parseL(p[19]) };
            const int64_t exp = parseL(p[20]);
            const int64_t got = pp.fitness(tp);
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ClimateMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
