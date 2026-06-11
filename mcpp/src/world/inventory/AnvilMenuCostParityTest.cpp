// Parity test for the PURE static cost helper of
// net.minecraft.world.inventory.AnvilMenu — verifies the C++ port in
// world/inventory/AnvilMenuCost.h reproduces the REAL static bit-for-bit against
// tools/AnvilMenuCostParity.java ground truth.
//
//   anvil_cost_parity --cases mcpp/build/anvil_cost.tsv
//
// TSV row (see AnvilMenuCostParity.java):
//   COST  <baseCost>  <result>
// Both fields are decimal int32 (the method takes and returns a Java int).

#include "AnvilMenuCost.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace inv = mc::world::inventory;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: anvil_cost_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "COST") {
            // COST <baseCost> <result>
            int32_t baseCost = toI(p[1]);
            int32_t exp = toI(p[2]);
            int32_t got = inv::calculateIncreasedRepairCost(baseCost);
            if (got != exp)
                fail(line + " got=" + std::to_string(got) + " exp=" + std::to_string(exp));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "AnvilMenuCost checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
