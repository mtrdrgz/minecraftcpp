// Parity test for net.minecraft.world.item.ItemDisplayContext. Ground truth:
// tools/ItemDisplayContextParity.java vs the real enum.
//
// Verifies, bit-for-bit, every ItemDisplayContext constant's
// (ordinal, id, serializedName, firstPerson, leftHand), plus the full
// ItemDisplayContext.BY_ID (ByIdMap.continuous / ZERO) lookup across a battery of
// int keys.
//
//   item_display_context_parity --cases mcpp/build/item_display_context.tsv

#include "ItemDisplayContext.h"

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
    if (casesPath.empty()) {
        std::cerr << "usage: item_display_context_parity --cases <tsv>\n";
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

        if (t == "CTX") {
            // CTX <ordinal> <id> <serializedName> <firstPerson> <leftHand>
            const int ordinal = i(p[1]);
            if (ordinal < 0 || ordinal >= item::ITEM_DISPLAY_CONTEXT_COUNT) {
                fail(line + " ordinal-out-of-range");
                continue;
            }
            const auto c = static_cast<item::ItemDisplayContext>(ordinal);
            eqI(item::ordinal(c), p[1], line);
            eqI(item::getId(c), p[2], line);
            eqS(std::string(item::getSerializedName(c)), p[3], line);
            eqI(item::firstPerson(c) ? 1 : 0, p[4], line);
            eqI(item::leftHand(c) ? 1 : 0, p[5], line);
        } else if (t == "BYID") {
            // BYID <id> <ordinal>; byId(id) -> resolved context, compare its ordinal.
            const int id = i(p[1]);
            const item::ItemDisplayContext c = item::byId(id);
            eqI(item::ordinal(c), p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ItemDisplayContext cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
