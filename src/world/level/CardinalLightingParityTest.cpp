// Parity gate for mc::world::level::cardinal (CardinalLighting) — per-face directional shading
// multipliers. Bit-exact float compare vs the real DEFAULT/NETHER records.
//
//   cardinal_lighting_parity --cases mcpp/build/cardinal_lighting.tsv

#include "CardinalLighting.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cl = mc::world::level::cardinal;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: cardinal_lighting_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "CL") continue;
        ++n;
        const cl::CardinalLighting& c = (p[1] == "NETHER") ? cl::NETHER : cl::DEFAULT;
        int dir = std::stoi(p[2]);
        uint32_t expByFace = static_cast<uint32_t>(std::stoul(p[3], nullptr, 16));
        uint32_t expUp = static_cast<uint32_t>(std::stoul(p[4], nullptr, 16));
        if (fbits(c.byFace(dir)) != expByFace || fbits(c.up) != expUp) {
            ++bad;
            if (shown++ < 20) std::cerr << "MISMATCH " << p[1] << " dir=" << dir << "\n";
        }
    }
    std::cout << "CardinalLighting cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
