// Dedicated parity gate for net.minecraft.util.LightCoordsUtil (Minecraft 26.1.2).
//
// The C++ port is REUSED (not duplicated): it lives in util/Brightness.h, namespace
// mc::util::lightcoords. This test #includes that certified header and recomputes every
// LightCoordsUtil method against ground truth from tools/LightCoordsUtilParity.java
// (which calls the REAL net.minecraft.util.LightCoordsUtil). Compared bit-for-bit:
// ints as decimal (signed 32-bit), floats parsed from raw IEEE-754 bits.
//
//   light_coords_util_parity --cases mcpp/build/light_coords_util.tsv

#include "Brightness.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace lc = mc::util::lightcoords;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints are signed 32-bit; some print as negative (e.g. 0x80000000 -> -2147483648).
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
// Float from raw IEEE-754 hex bits.
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: light_coords_util_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "PACK") {  // block sky | packed
            bad = lc::pack(i(p[1]), i(p[2])) != i(p[3]);
        } else if (tag == "SMOOTHPACK") {  // block sky | packed
            bad = lc::smoothPack(i(p[1]), i(p[2])) != i(p[3]);
        } else if (tag == "BLOCK") {  // packed | block
            bad = lc::block(i(p[1])) != i(p[2]);
        } else if (tag == "SKY") {  // packed | sky
            bad = lc::sky(i(p[1])) != i(p[2]);
        } else if (tag == "SMOOTHBLOCK") {  // packed | out
            bad = lc::smoothBlock(i(p[1])) != i(p[2]);
        } else if (tag == "SMOOTHSKY") {  // packed | out
            bad = lc::smoothSky(i(p[1])) != i(p[2]);
        } else if (tag == "WITHBLOCK") {  // coords block | out
            bad = lc::withBlock(i(p[1]), i(p[2])) != i(p[3]);
        } else if (tag == "ADDEMIT") {  // lightCoords emissionBits | out
            bad = lc::addSmoothBlockEmission(i(p[1]), bf(p[2])) != i(p[3]);
        } else if (tag == "MAX") {  // coords1 coords2 | out
            bad = lc::max(i(p[1]), i(p[2])) != i(p[3]);
        } else if (tag == "EMIT") {  // lightCoords emission | out
            bad = lc::lightCoordsWithEmission(i(p[1]), i(p[2])) != i(p[3]);
        } else if (tag == "SMOOTHBLEND") {  // n1 n2 n3 center | out
            bad = lc::smoothBlend(i(p[1]), i(p[2]), i(p[3]), i(p[4])) != i(p[5]);
        } else if (tag == "SMOOTHWBLEND") {  // c1 c2 c3 c4 w1 w2 w3 w4 | out
            bad = lc::smoothWeightedBlend(i(p[1]), i(p[2]), i(p[3]), i(p[4]),
                                          bf(p[5]), bf(p[6]), bf(p[7]), bf(p[8])) != i(p[9]);
        } else if (tag == "CONST") {  // FULL_BRIGHT FULL_SKY MAX_SMOOTH(=240)
            bad = lc::FULL_BRIGHT != i(p[1]) || lc::FULL_SKY != i(p[2]) ||
                  lc::MAX_SMOOTH_LIGHT_LEVEL != i(p[3]);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "LightCoordsUtil cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
