// Parity test for net.minecraft.util.CommonColors. Ground truth:
// tools/CommonColorsParity.java reflection-dumps the real class's named ARGB int
// constants; this verifies each NAME -> int value bit-for-bit against util/CommonColors.h.
//
//   common_colors_parity --cases mcpp/build/common_colors.tsv

#include "CommonColors.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace cc = mc::common_colors;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// All named constants from the real class, by name. Values come from util/CommonColors.h.
const std::unordered_map<std::string, std::int32_t>& table() {
    static const std::unordered_map<std::string, std::int32_t> m = {
        {"WHITE", cc::WHITE},
        {"BLACK", cc::BLACK},
        {"GRAY", cc::GRAY},
        {"DARK_GRAY", cc::DARK_GRAY},
        {"LIGHT_GRAY", cc::LIGHT_GRAY},
        {"LIGHTER_GRAY", cc::LIGHTER_GRAY},
        {"RED", cc::RED},
        {"SOFT_RED", cc::SOFT_RED},
        {"GREEN", cc::GREEN},
        {"BLUE", cc::BLUE},
        {"YELLOW", cc::YELLOW},
        {"SOFT_YELLOW", cc::SOFT_YELLOW},
        {"DARK_PURPLE", cc::DARK_PURPLE},
        {"HIGH_CONTRAST_DIAMOND", cc::HIGH_CONTRAST_DIAMOND},
        {"COSMOS_PINK", cc::COSMOS_PINK},
        {"TEXT_GRAY", cc::TEXT_GRAY},
    };
    return m;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: common_colors_parity --cases <tsv>\n";
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
        if (p[0] != "COLOR") {
            fail("UNKNOWN_TAG " + p[0]);
            ++total;
            continue;
        }
        ++total;
        const std::string& name = p[1];
        // Bit-exact: ground truth value parsed as signed 32-bit, compared via raw bits.
        std::int32_t exp = static_cast<std::int32_t>(std::stoll(p[2]));
        auto it = table().find(name);
        if (it == table().end()) {
            fail(line + " (no such constant in C++ header)");
            continue;
        }
        std::int32_t got = it->second;
        if (std::bit_cast<std::uint32_t>(got) != std::bit_cast<std::uint32_t>(exp))
            fail(line + " got=" + std::to_string(got));
    }

    // Also ensure no C++ constant was left untested by the ground truth (missing row).
    if (total != static_cast<long long>(table().size())) {
        std::cerr << "WARNING: row count " << total << " != C++ constant count "
                  << table().size() << " (a constant may be untested or duplicated)\n";
    }

    std::cout << "CommonColors cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
