// Bit-exact parity gate for the C++ port of the PURE static math helpers of
//   net.minecraft.world.level.levelgen.feature.trunkplacers.FancyTrunkPlacer
//   (mcpp/src/world/level/levelgen/feature/trunkplacers/FancyTrunkMath.h).
//
// Header-only — no link deps. Ground truth from tools/FancyTrunkMathParity.java
// drives the REAL decompiled FancyTrunkPlacer (treeShape / getSteps / getLogAxis /
// trimBranches) via reflection; this test recomputes each row and compares.
//
//   default        -> a handful of hardcoded self-checks (no Mojang files needed)
//   --cases <tsv>  -> verify every row of the generated reference
//
// Row tags (see the Java tool):
//   TS  <height>  <y>          <treeShape_bits>        (float, raw 32-bit hex)
//   ST  <dx> <dy> <dz>         <getSteps_int>          (int decimal)
//   LA  <sx> <sz> <bx> <bz>    <axisOrdinal>           (X=0 Y=1 Z=2)
//   TB  <height> <localY>      <0|1>

#include "FancyTrunkMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ftm = mc::levelgen::trunkplacers;

namespace {

float bitsToFloat(const std::string& hex) {
    const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    return std::bit_cast<float>(bits);
}

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;

    if (tag == "TS") {
        int height = 0, y = 0;
        std::string expHex;
        in >> height >> y >> expHex;
        const int32_t expected = std::bit_cast<int32_t>(bitsToFloat(expHex));
        const int32_t got = std::bit_cast<int32_t>(ftm::treeShape(height, y));
        if (got != expected) {
            err = "TS h=" + std::to_string(height) + " y=" + std::to_string(y) +
                  " bits " + std::to_string(got) + "!=" + std::to_string(expected);
            return false;
        }
        return true;
    }
    if (tag == "ST") {
        int dx = 0, dy = 0, dz = 0;
        long long expected = 0;
        in >> dx >> dy >> dz >> expected;
        const int32_t got = ftm::getSteps(dx, dy, dz);
        if (static_cast<long long>(got) != expected) {
            err = "ST (" + std::to_string(dx) + "," + std::to_string(dy) + "," +
                  std::to_string(dz) + ") " + std::to_string(got) +
                  "!=" + std::to_string(expected);
            return false;
        }
        return true;
    }
    if (tag == "LA") {
        int sx = 0, sz = 0, bx = 0, bz = 0, expected = 0;
        in >> sx >> sz >> bx >> bz >> expected;
        const int32_t got = static_cast<int32_t>(ftm::getLogAxis(sx, sz, bx, bz));
        if (got != expected) {
            err = "LA start(" + std::to_string(sx) + "," + std::to_string(sz) +
                  ") block(" + std::to_string(bx) + "," + std::to_string(bz) +
                  ") axis " + std::to_string(got) + "!=" + std::to_string(expected);
            return false;
        }
        return true;
    }
    if (tag == "TB") {
        int height = 0, localY = 0, expected = 0;
        in >> height >> localY >> expected;
        const int got = ftm::trimBranches(height, localY) ? 1 : 0;
        if (got != expected) {
            err = "TB h=" + std::to_string(height) + " ly=" + std::to_string(localY) +
                  " " + std::to_string(got) + "!=" + std::to_string(expected);
            return false;
        }
        return true;
    }

    err = "unknown tag " + tag;
    return false;
}

// A few hardcoded rows (regenerate via tools/FancyTrunkMathParity.java if the
// formula ever changes). Format identical to the TSV.
const std::vector<std::string> kHardcoded = {
    // treeShape: height=7,y=2 — the float-early-out trap. 7*0.3f=2.1f, 2.0f<2.1f
    // is TRUE so the function returns -1.0F (bits bf800000). An int-truncated
    // port (2 < (int)2.1 == 2 -> false) would instead fall through and FAIL here.
    "TS\t7\t2\tbf800000",
    // treeShape: height=10,y=5 -> radius=5, adjacent=0 -> distance=radius=5 ->
    // return 2.5F (bits 40200000).
    "TS\t10\t5\t40200000",
    // treeShape: height=12,y=3 (3 >= 12*0.3f=3.6f? -> 3.0<3.6 true -> -1.0F).
    "TS\t12\t3\tbf800000",
    // getSteps: max(|3|,|−5|,|2|) = 5.
    "ST\t3\t-5\t2\t5",
    // getSteps: abs(INT_MIN) overflow stays INT_MIN; max picks it (it is the only
    // negative kept as-is) vs 0,0 -> INT_MIN. max(INT_MIN,0,0)=0 actually -> 0.
    "ST\t-2147483648\t0\t0\t0",
    // getLogAxis: equal nonzero xdiff/zdiff -> tie picks X (ordinal 0).
    "LA\t0\t0\t3\t3\t0",
    // getLogAxis: zero diffs -> Y (ordinal 1).
    "LA\t5\t5\t5\t5\t1",
    // getLogAxis: zdiff>xdiff -> Z (ordinal 2).
    "LA\t0\t0\t1\t9\t2",
    // trimBranches: localY=2 >= 10*0.2=2.0 -> true.
    "TB\t10\t2\t1",
    // trimBranches: localY=1 >= 10*0.2=2.0 -> false.
    "TB\t10\t1\t0",
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n';
            }
        }
        std::cout << "FancyTrunkMath checks=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    long n = 0, bad = 0;
    for (const auto& line : kHardcoded) {
        std::string err;
        ++n;
        if (!verifyLine(line, err)) { ++bad; std::cerr << "FAIL: " << err << '\n'; }
    }
    std::cout << "FancyTrunkMath checks=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
