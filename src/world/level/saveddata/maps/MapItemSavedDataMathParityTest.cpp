// Parity test for the pure static map coordinate/decoration math of
// mc::world::level::saveddata::maps (MapItemSavedDataMath.h) vs Java ground truth.
//
// Reads the TSV emitted by tools/MapItemSavedDataMathParity.java and recomputes
// each function on the C++ side, comparing bit-for-bit:
//   - floats are exchanged as raw IEEE-754 bits (Float.floatToRawIntBits <-> bit_cast)
//   - doubles as raw bits (Double.doubleToRawLongBits <-> bit_cast)
//   - ints / bytes as decimal
//
// Build (from repo root, llvm-mingw on PATH):
//   clang++ -O2 -std=c++20 -I mcpp/src \
//     mcpp/src/world/level/saveddata/maps/MapItemSavedDataMathParityTest.cpp \
//     -o mcpp/build/map_item_saved_data_math_probe.exe
//   mcpp/build/map_item_saved_data_math_probe.exe --cases mcpp/build/map_item_saved_data_math.tsv
#include "world/level/saveddata/maps/MapItemSavedDataMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace maps = mc::world::level::saveddata::maps;

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

static float f_from_bits(const std::string& s) {
    // Java prints signed 32-bit decimals; parse as long then narrow.
    std::int32_t bits = static_cast<std::int32_t>(std::stol(s));
    return std::bit_cast<float>(bits);
}
static double d_from_bits(const std::string& s) {
    std::int64_t bits = static_cast<std::int64_t>(std::stoll(s));
    return std::bit_cast<double>(bits);
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: MapItemSavedDataMathParityTest --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CENTER") {
            // CENTER <originXbits> <originYbits> <scale> <centerX> <centerZ>
            ++cases;
            double ox = d_from_bits(p[1]);
            double oy = d_from_bits(p[2]);
            std::int32_t scale = std::stoi(p[3]);
            std::int32_t expX = std::stoi(p[4]);
            std::int32_t expZ = std::stoi(p[5]);
            maps::MapCenter got = maps::calculateMapCenter(ox, oy, scale);
            if (got.centerX != expX || got.centerZ != expZ) {
                ++mism;
                std::cerr << "CENTER mismatch ox=" << ox << " oy=" << oy
                          << " scale=" << scale
                          << " expected=(" << expX << "," << expZ << ")"
                          << " got=(" << got.centerX << "," << got.centerZ << ")\n";
            }
        } else if (tag == "INSIDE") {
            // INSIDE <xdBits> <ydBits> <0|1>
            ++cases;
            float xd = f_from_bits(p[1]);
            float yd = f_from_bits(p[2]);
            bool exp = std::stoi(p[3]) != 0;
            bool got = maps::isInsideMap(xd, yd);
            if (got != exp) {
                ++mism;
                std::cerr << "INSIDE mismatch xd=" << xd << " yd=" << yd
                          << " expected=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "CLAMP") {
            // CLAMP <deltaBits> <byteResult>
            ++cases;
            float d = f_from_bits(p[1]);
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(maps::clampMapCoordinate(d));
            if (got != exp) {
                ++mism;
                std::cerr << "CLAMP mismatch delta=" << d
                          << " expected=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "ROT") {
            // ROT <yRotBits> <byteResult>
            ++cases;
            double y = d_from_bits(p[1]);
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(maps::calculateRotationOverworld(y));
            if (got != exp) {
                ++mism;
                std::cerr << "ROT mismatch yRot=" << y
                          << " expected=" << exp << " got=" << got << "\n";
            }
        } else if (tag == "DELTA") {
            // DELTA <worldBits> <centerBits> <scale> <resultBits>
            ++cases;
            double w = d_from_bits(p[1]);
            double c = d_from_bits(p[2]);
            std::int32_t scale = std::stoi(p[3]);
            float exp = f_from_bits(p[4]);
            float got = maps::decorationDeltaFromCenter(w, c, scale);
            // Bit-exact compare (handles signed zero / NaN payload faithfully).
            if (std::bit_cast<std::uint32_t>(got) != std::bit_cast<std::uint32_t>(exp)) {
                ++mism;
                std::cerr << "DELTA mismatch world=" << w << " center=" << c
                          << " scale=" << scale
                          << " expectedBits=" << std::bit_cast<std::int32_t>(exp)
                          << " gotBits=" << std::bit_cast<std::int32_t>(got) << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "MapItemSavedDataMath checks=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
