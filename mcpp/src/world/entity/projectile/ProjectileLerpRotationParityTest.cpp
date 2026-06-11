// Parity test for Projectile.lerpRotation. Ground truth:
// tools/ProjectileLerpRotationParity.java. Bit-exact (floats as raw IEEE-754 bits).
//
//   projectile_lerp_rotation_parity --cases mcpp/build/projectile_lerp_rotation.tsv

#include "ProjectileLerpRotation.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: projectile_lerp_rotation_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        ++total;
        if (p[0] == "LERP") {
            float rotO = bf(p[1]);
            float rot = bf(p[2]);
            float r = mc::projectileLerpRotation(rotO, rot);
            if (fb(r) != static_cast<uint32_t>(std::stoul(p[3], nullptr, 16))) {
                ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << line << " got=" << std::hex << fb(r) << std::dec << "\n";
            }
        } else { ++mism; if (shown++ < 30) std::cerr << "UNKNOWN_TAG " << p[0] << "\n"; }
    }
    std::cout << "ProjectileLerpRotation checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
