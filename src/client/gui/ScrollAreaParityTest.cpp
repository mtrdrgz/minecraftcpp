// Parity gate for mc::gui::ScrollArea vs the real net.minecraft.client.gui.components.
// AbstractScrollArea scroll/scrollbar math. Doubles arrive as doubleToRawLongBits and are compared
// bit-for-bit; ints exactly.
//
//   scroll_area_parity --cases mcpp/build/scroll_area.tsv

#include "ScrollArea.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gui = mc::gui;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int I(const std::string& s) { return std::stoi(s); }
long long L(const std::string& s) { return std::stoll(s); }
double D(const std::string& s) { return std::bit_cast<double>(static_cast<std::uint64_t>(std::stoll(s))); }

gui::ScrollArea mk(const std::vector<std::string>& p) {
    gui::ScrollArea a;
    a.x = I(p[1]); a.y = I(p[2]); a.width = I(p[3]); a.height = I(p[4]);
    a.contentHeight_ = I(p[5]); a.scrollRate_ = static_cast<double>(I(p[6]));
    a.scrollbarWidth_ = 6;
    return a;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: scroll_area_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };
    auto cfg = [&](const std::vector<std::string>& p) {
        return "(" + p[1] + "," + p[2] + "," + p[3] + "," + p[4] + " ch=" + p[5] + " r=" + p[6] + ")";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        auto a = mk(p);
        if (t == "AREA") {
            ++n;
            bool ok = a.maxScrollAmount() == I(p[7]) && (int)a.scrollable() == I(p[8]) &&
                      a.scrollerHeight() == I(p[9]) && a.scrollBarX() == I(p[10]) &&
                      a.scrollbarWidth() == I(p[11]) &&
                      std::bit_cast<std::uint64_t>(a.scrollRate()) == (std::uint64_t)L(p[12]);
            if (!ok) fail("AREA " + cfg(p));
        } else if (t == "SCROLL") {
            ++n;
            a.setScrollAmount(D(p[7]));
            bool ok = std::bit_cast<std::uint64_t>(a.scrollAmount()) == (std::uint64_t)L(p[8]) &&
                      a.scrollBarY() == I(p[9]);
            if (!ok) fail("SCROLL " + cfg(p) + " set=" + p[7]);
        } else if (t == "OVER") {
            ++n;
            if ((int)a.isOverScrollbar(D(p[7]), D(p[8])) != I(p[9]))
                fail("OVER " + cfg(p));
        } else if (t == "WHEEL") {
            ++n;
            a.setScrollAmount(D(p[7]));
            a.mouseScrolled(D(p[8]));
            if (std::bit_cast<std::uint64_t>(a.scrollAmount()) != (std::uint64_t)L(p[9]))
                fail("WHEEL " + cfg(p) + " base=" + p[7] + " w=" + p[8]);
        }
    }
    std::cout << "ScrollArea checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
