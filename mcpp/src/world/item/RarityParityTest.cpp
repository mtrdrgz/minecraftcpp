// Parity test for net.minecraft.world.item.Rarity. Ground truth:
// tools/RarityParity.java vs the real enum + its real ChatFormatting color.
//
// Verifies, bit-for-bit, every Rarity constant's (ordinal, id, serializedName)
// and its color's (ordinal, name, char code, id, color value), plus the full
// Rarity.BY_ID (ByIdMap.continuous / ZERO) lookup across a battery of int keys.
//
//   rarity_parity --cases mcpp/build/rarity.tsv

#include "Rarity.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace item = mc::world::item;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints printed as signed decimal; parse via long long then narrow.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: rarity_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqS = [&](const std::string& got, const std::string& exp, const std::string& l) {
        if (got != exp) fail(l + " got=" + got);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "RARITY") {
            // RARITY <ordinal> <id> <serializedName> <colorOrdinal> <colorName>
            //        <colorCode> <colorId> <colorValue>
            const int ordinal = i(p[1]);
            if (ordinal < 0 || ordinal >= item::RARITY_COUNT) {
                fail(line + " ordinal-out-of-range");
                continue;
            }
            const item::RarityData& r =
                item::RARITIES[static_cast<std::size_t>(ordinal)];
            eqI(item::getId(r), p[2], line);
            eqS(std::string(item::getSerializedName(r)), p[3], line);
            const item::ChatFormattingRef& c = item::color(r);
            eqI(c.ordinal, p[4], line);
            eqS(std::string(c.enumName), p[5], line);
            eqI(static_cast<int>(static_cast<unsigned char>(c.code)), p[6], line);
            eqI(c.id, p[7], line);
            eqI(c.color, p[8], line);
        } else if (t == "BYID") {
            // BYID <id> <ordinal>; byId(id) -> resolved RarityData, ordinal == its id.
            const int id = i(p[1]);
            const item::RarityData& r = item::byId(id);
            // RarityData carries no stored ordinal; declaration order == id order,
            // so the resolved id is the ordinal.
            eqI(r.id, p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "Rarity cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
