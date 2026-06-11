// Parity test for mcpp/src/client/particle/LifetimeAlpha.h — the pure alpha-over-lifetime
// math of net.minecraft.client.particle.Particle.LifetimeAlpha (Minecraft 26.1.2).
//
// Ground truth: tools/LifetimeAlphaParity.java, which drives the REAL record
// (currentAlphaForAge / isOpaque). Floats are compared BIT-FOR-BIT via std::bit_cast.
//
//   lifetime_alpha_parity --cases mcpp/build/lifetime_alpha.tsv

#include "LifetimeAlpha.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cp = mc::client::particle;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: lifetime_alpha_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "ALPHA") {
            // ALPHA start end startAge endAge age lifetime pt | out
            if (p.size() < 9) continue;
            cp::LifetimeAlpha la{bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4])};
            int age = std::stoi(p[5]);
            int lifetime = std::stoi(p[6]);
            float pt = bf(p[7]);
            uint32_t expect = static_cast<uint32_t>(std::stoul(p[8], nullptr, 16));
            uint32_t got = fb(la.currentAlphaForAge(age, lifetime, pt));
            ++checks;
            if (got != expect) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "ALPHA s=" << p[1] << " e=" << p[2] << " w=[" << p[3] << "," << p[4]
                              << "] age=" << age << " life=" << lifetime << " pt=" << p[7]
                              << " expect=" << p[8] << " got=" << std::hex << got << std::dec << "\n";
            }
        } else if (tag == "OPAQUE") {
            // OPAQUE start end startAge endAge | out(0/1)
            if (p.size() < 6) continue;
            cp::LifetimeAlpha la{bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4])};
            int expect = std::stoi(p[5]);
            int got = la.isOpaque() ? 1 : 0;
            ++checks;
            if (got != expect) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "OPAQUE s=" << p[1] << " e=" << p[2]
                              << " expect=" << expect << " got=" << got << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "LifetimeAlpha checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
