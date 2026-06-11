// Parity test for mcpp/src/client/renderer/entity/EndCrystalBeamMath.h — the pure
// beam-height oscillator EndCrystalRenderer.getY(float) of
// net.minecraft.client.renderer.entity.EndCrystalRenderer (Minecraft 26.1.2).
//
// Ground truth: tools/EndCrystalBeamParity.java, which calls the REAL static
// EndCrystalRenderer.getY over a broad sweep of finite, physical `timeInTicks` floats
// and emits the raw IEEE-754 bits of each result. Here we replay the same inputs
// through mc::client::renderer::entity::getY and compare BIT-FOR-BIT.
//
//   end_crystal_beam_parity --cases mcpp/build/end_crystal_beam.tsv
//
// Each row:  GETY  timeInTicksBits(f)  outBits(f)

#include "EndCrystalBeamMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ce = mc::client::renderer::entity;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: end_crystal_beam_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "GETY") {
            // GETY timeInTicksBits outBits
            if (p.size() < 3) continue;
            float timeInTicks = bf(p[1]);
            uint32_t expect = static_cast<uint32_t>(std::stoul(p[2], nullptr, 16));

            uint32_t got = fb(ce::getY(timeInTicks));
            ++checks;
            if (got != expect) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << tag << " timeInTicks=" << timeInTicks
                              << " expect=" << std::hex << expect << " got=" << got
                              << std::dec << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "EndCrystalBeam checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
