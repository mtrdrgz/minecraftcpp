// Parity test for net.minecraft.world.item.Item durability-bar display math (26.1.2):
//   Item.isBarVisible / Item.getBarWidth / Item.getBarColor, flowing through
//   ItemStack.getDamageValue()'s clamp. Ground truth from tools/ItemBarDisplayParity.java
//   drives the REAL classes over every damageable vanilla item x a stored-damage sweep.
//
// All values compared bit/decimal exact (barColor is an ARGB int from Mth.hsvToRgb).
//
//   item_bar_display_parity --cases mcpp/build/item_bar_display.tsv

#include "world/item/ItemBarDisplay.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ibd = mc::item;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t i32(const std::string& s) { return static_cast<int32_t>(std::stol(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: item_bar_display_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& msg, const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << " | " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "BAR") {
            // BAR <storedDamage> <maxDamage> <barWidth> <barColor> <barVisible 0/1>
            int32_t storedDamage = i32(p[1]);
            int32_t maxDamage    = i32(p[2]);
            int32_t expWidth     = i32(p[3]);
            int32_t expColor     = i32(p[4]);
            bool    expVisible   = i32(p[5]) != 0;

            // damageable == true: every emitted item is damageable by construction
            // on the Java side (filtered to isDamageableItem()).
            int32_t gotWidth   = ibd::getBarWidth(storedDamage, maxDamage);
            int32_t gotColor   = ibd::getBarColor(storedDamage, maxDamage);
            bool    gotVisible = ibd::isBarVisible(storedDamage, maxDamage, /*damageable=*/true);

            if (gotWidth != expWidth)
                fail("barWidth got=" + std::to_string(gotWidth) + " exp=" + std::to_string(expWidth), line);
            if (gotColor != expColor)
                fail("barColor got=" + std::to_string(gotColor) + " exp=" + std::to_string(expColor), line);
            if (gotVisible != expVisible)
                fail("barVisible got=" + std::to_string(gotVisible) + " exp=" + std::to_string(expVisible), line);
        } else {
            fail("unknown TAG " + t, line);
        }
    }

    std::cout << "ItemBarDisplay checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
