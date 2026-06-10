// Parity test for net.minecraft.world.entity.Crackiness (26.1.2).
// Ground truth: tools/CrackinessParity.java.
//
//   crackiness_parity --cases <crackiness.tsv>
//
// Each row carries the exact byte pattern of the input (float fractions as
// 8-hex raw int bits, ints decimal) plus the expected Crackiness.Level ordinal.
// The C++ side reconstructs the input bit-for-bit (std::bit_cast on the hex),
// recomputes byFraction / byDamage on the matching preset, and compares the
// resulting Level ordinal exactly.
//
// Row tags:
//   FRAC  <preset> <fraction8>          <levelOrdinal>
//   DAMG  <preset> <damage> <maxDamage> <levelOrdinal>

#include "world/entity/Crackiness.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Crackiness;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}

const Crackiness& preset(const std::string& name) {
    if (name == "GOLEM") return mc::CRACKINESS_GOLEM;
    return mc::CRACKINESS_WOLF_ARMOR; // "WOLF_ARMOR"
}
int ord(Crackiness::Level l) { return static_cast<int>(l); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: crackiness_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "FRAC") {
            // FRAC <preset> <fraction8> <ord>
            if (p.size() != 4) { fail("BADROW " + line); continue; }
            const Crackiness& c = preset(p[1]);
            float fr = bf(p[2]);
            int want = std::stoi(p[3]);
            int got = ord(c.byFraction(fr));
            if (got != want) fail(line);
        } else if (tag == "DAMG") {
            // DAMG <preset> <damage> <maxDamage> <ord>
            if (p.size() != 5) { fail("BADROW " + line); continue; }
            const Crackiness& c = preset(p[1]);
            int32_t dmg = static_cast<int32_t>(std::stol(p[2]));
            int32_t max = static_cast<int32_t>(std::stol(p[3]));
            int want = std::stoi(p[4]);
            int got = ord(c.byDamage(dmg, max));
            if (got != want) fail(line);
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "Crackiness cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
