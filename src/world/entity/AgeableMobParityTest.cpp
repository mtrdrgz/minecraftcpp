// Parity test for the pure static helper of AgeableMob (Minecraft 26.1.2):
//   AgeableMob.getSpeedUpSecondsWhenFeeding(int)  ==  (int)(t / 20 * 0.1F)
//
// Ground truth: tools/AgeableMobParity.java drives the REAL net.minecraft class
// via reflection -> TSV; this test re-runs the C++ port (AgeableMob.h) on the
// same inputs and compares exactly. The result is a plain int (fits in 32 bits),
// so the comparison is decimal/exact — no float bit exchange needed (the float
// arithmetic happens inside the port and is validated by the integer outcome).
//
//   ageable_mob_parity --cases mcpp/build/ageable_mob.tsv

#include "world/entity/AgeableMob.h"

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
// narrow to int — the inputs are valid 32-bit ints by construction.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: ageable_mob_parity --cases <tsv>\n"; return 2; }
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

        if (t == "FEED") {
            int got = mc::world::entity::getSpeedUpSecondsWhenFeeding(i(p[1]));
            if (got != i(p[2])) fail(line + " got=" + std::to_string(got));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "AgeableMob checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
