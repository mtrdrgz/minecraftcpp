// Parity test for Entity.getInputVector. Ground truth: tools/EntityMovementParity.java.
// Bit-exact (doubles/floats as raw IEEE-754 bits).
//
//   entity_movement_parity --cases mcpp/build/entity_movement.tsv

#include "EntityMovement.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float  bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: entity_movement_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        ++total;
        if (p[0] == "INPUTVEC") {
            Vec3 r = mc::getInputVector(Vec3{bd(p[1]), bd(p[2]), bd(p[3])}, bf(p[4]), bf(p[5]));
            if (db(r.x) != std::stoull(p[6], nullptr, 16) || db(r.y) != std::stoull(p[7], nullptr, 16) || db(r.z) != std::stoull(p[8], nullptr, 16)) {
                ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << line << "\n";
            }
        } else if (p[0] == "VIEWVEC" || p[0] == "UPVEC") {
            Vec3 r = p[0] == "VIEWVEC" ? mc::calculateViewVector(bf(p[1]), bf(p[2])) : mc::calculateUpVector(bf(p[1]), bf(p[2]));
            if (db(r.x) != std::stoull(p[3], nullptr, 16) || db(r.y) != std::stoull(p[4], nullptr, 16) || db(r.z) != std::stoull(p[5], nullptr, 16)) {
                ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << line << "\n";
            }
        } else { ++mism; if (shown++ < 30) std::cerr << "UNKNOWN_TAG " << p[0] << "\n"; }
    }
    std::cout << "EntityMovement cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
