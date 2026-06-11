// Parity test for the PURE static helper
//   net.minecraft.world.entity.animal.happyghast.HappyGhast$HappyGhastLookControl
//       .wrapDegrees90(float)
// Verifies the C++ port in world/entity/animal/happyghast/HappyGhastWrapDegrees90.h
// reproduces the REAL method bit-for-bit against
// tools/HappyGhastWrapDegrees90Parity.java ground truth.
//
//   happy_ghast_wrap_degrees90_parity --cases mcpp/build/happy_ghast_wrap_degrees90.tsv
//
// TSV rows (see HappyGhastWrapDegrees90Parity.java):
//   WRAP90  <angleBits>  <resultBits>
// Both fields are the raw IEEE-754 int32 bit patterns of the float in/out. We bit-cast
// the input bits back to a float, run the port, and require the result's raw bits to
// match exactly (so NaN / signed-zero / infinity are all checked bit-for-bit).

#include "HappyGhastWrapDegrees90.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace hg = mc::world::entity::animal::happyghast;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

float bitsToFloat(int32_t bits) {
    float f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}
int32_t floatToBits(float f) {
    int32_t bits;
    std::memcpy(&bits, &f, sizeof(bits));
    return bits;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: happy_ghast_wrap_degrees90_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        if (p[0] != "WRAP90") {
            ++mism;
            if (shown++ < 40) std::cerr << "MISMATCH UNKNOWN_TAG " << p[0] << "\n";
            continue;
        }
        // WRAP90 <angleBits> <resultBits>
        ++total;
        int32_t angleBits = toI(p[1]);
        int32_t expBits = toI(p[2]);
        float in_f = bitsToFloat(angleBits);
        int32_t gotBits = floatToBits(hg::wrapDegrees90(in_f));
        if (gotBits != expBits) {
            ++mism;
            if (shown++ < 40)
                std::cerr << "MISMATCH " << line << " gotBits=" << gotBits
                          << " expBits=" << expBits << "\n";
        }
    }

    std::cout << "HappyGhastWrapDegrees90 checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
