// Parity test for the two pure XP integer formulas (Minecraft 26.1.2):
//   Player.getXpNeededForNextLevel()  and  ExperienceOrb.getExperienceValue(int).
// Ground truth: tools/PlayerXpParity.java vs the real classes. Pure int math,
// compared exactly (every value fits in int; no float exchange needed).
//
//   player_xp_parity --cases mcpp/build/player_xp.tsv

#include "world/entity/player/PlayerXp.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// TSV ints are plain decimal (may be INT_MIN/INT_MAX). Parse as long long then
// narrow to int — the inputs themselves are valid 32-bit ints by construction.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: player_xp_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "XPNEED") {
            int got = mc::world::entity::player::getXpNeededForNextLevel(i(p[1]));
            if (got != i(p[2])) fail(line + " got=" + std::to_string(got));
        }
        else if (t == "XPVAL") {
            int got = mc::world::entity::player::getExperienceValue(i(p[1]));
            if (got != i(p[2])) fail(line + " got=" + std::to_string(got));
        }
        else { fail("UNKNOWN_TAG " + t); }
    }

    std::cout << "PlayerXp cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
