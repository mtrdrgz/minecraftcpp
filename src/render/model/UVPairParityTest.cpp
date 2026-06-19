// Parity gate for mc::render::model::uvpair (port of UVPair.pack/unpackU/unpackV) vs the
// real net.minecraft.client.model.geom.builders.UVPair, bit-for-bit.
//   uvpair_parity --cases mcpp/build/uvpair.tsv
#include "UVPair.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

int main(int argc, char** argv) {
    std::string cases;
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") cases = argv[++i];
    if (cases.empty()) { std::cerr << "usage: --cases <tsv>\n"; return 2; }
    std::ifstream in(cases);
    if (!in) { std::cerr << "cannot open " << cases << "\n"; return 2; }

    auto fb = [](const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); };
    auto lb = [](const std::string& s) { return (uint64_t)std::stoull(s, nullptr, 16); };

    long n = 0, bad = 0; int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag; ss >> tag;
        if (tag == "PACK") {
            std::string uH, vH, pH; ss >> uH >> vH >> pH;
            ++n;
            uint64_t got = mc::render::model::uvpair::pack(fb(uH), fb(vH));
            if (got != lb(pH)) { ++bad; if (shown++ < 20) std::cerr << "PACK mismatch " << uH << "," << vH << "\n"; }
        } else if (tag == "UNPK") {
            std::string pH, uH, vH; ss >> pH >> uH >> vH;
            ++n;
            uint64_t p = lb(pH);
            uint32_t gu = std::bit_cast<uint32_t>(mc::render::model::uvpair::unpackU(p));
            uint32_t gv = std::bit_cast<uint32_t>(mc::render::model::uvpair::unpackV(p));
            if (gu != (uint32_t)std::stoul(uH, nullptr, 16) || gv != (uint32_t)std::stoul(vH, nullptr, 16)) {
                ++bad; if (shown++ < 20) std::cerr << "UNPK mismatch " << pH << "\n";
            }
        }
    }
    std::cout << "UVPair cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
