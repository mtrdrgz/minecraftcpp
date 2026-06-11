// Parity test for net.minecraft.world.entity.ExperienceOrb pure integer
// experience-decomposition helpers (26.1.2).
// Ground truth: tools/ExperienceOrbValueParity.java.
//
//   experience_orb_value_parity --cases <experience_orb_value.tsv>
//
// Rows (tab-separated):
//   XPVAL  <maxValue>          <getExperienceValue(maxValue)>
//   SPLIT  <amount>  <count>   <v0> <v1> ...
//
// ints are decimal. We reconstruct each int via std::stol -> int32_t (so
// INT_MIN/INT_MAX round-trip exactly), recompute on the C++ port, and compare
// bit-for-bit. SPLIT rows additionally compare the full orb-value sequence.

#include "world/entity/ExperienceOrbValue.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::ExperienceOrbValue;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t pi(const std::string& s) {
    return static_cast<int32_t>(std::stol(s));
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: experience_orb_value_parity --cases <tsv>\n";
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
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++total;

        if (tag == "XPVAL") {
            // XPVAL <maxValue> <expected>
            if (p.size() != 3) { fail("BADROW " + line); continue; }
            int32_t maxValue = pi(p[1]);
            int32_t want = pi(p[2]);
            int32_t got = ExperienceOrbValue::getExperienceValue(maxValue);
            if (got != want) fail(line);
        } else if (tag == "SPLIT") {
            // SPLIT <amount> <count> <v0> <v1> ...
            if (p.size() < 3) { fail("BADROW " + line); continue; }
            int32_t amount = pi(p[1]);
            int32_t count = pi(p[2]);
            if (static_cast<int32_t>(p.size()) != 3 + count) { fail("BADROW " + line); continue; }
            std::vector<int32_t> got = ExperienceOrbValue::splitIntoOrbs(amount);
            bool ok = (static_cast<int32_t>(got.size()) == count);
            for (int32_t i = 0; ok && i < count; ++i) {
                if (got[static_cast<size_t>(i)] != pi(p[static_cast<size_t>(3 + i)])) ok = false;
            }
            if (!ok) fail(line);
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "ExperienceOrbValue checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
