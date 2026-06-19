// Parity gate for mc::gui::WidgetGeometry vs the real net.minecraft.client.gui.components.
// AbstractWidget geometry/hit-testing/state core. Mouse coords arrive as doubleToRawLongBits so the
// exact double is reconstructed bit-for-bit (no parse rounding).
//
//   widget_geometry_parity --cases mcpp/build/widget_geometry.tsv

#include "WidgetGeometry.h"

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
double D(const std::string& s) { return std::bit_cast<double>(static_cast<std::uint64_t>(std::stoll(s))); }

gui::WidgetGeometry mk(const std::string& x, const std::string& y, const std::string& w,
                       const std::string& h, int active, int visible, int focused, int hovered) {
    gui::WidgetGeometry g;
    g.x = I(x); g.y = I(y); g.width = I(w); g.height = I(h);
    g.active = active != 0; g.visible = visible != 0;
    g.focused = focused != 0; g.isHovered = hovered != 0;
    return g;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: widget_geometry_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "WIDGET") {
            ++n;
            auto g = mk(p[1], p[2], p[3], p[4], I(p[5]), I(p[6]), I(p[7]), I(p[8]));
            bool ok = g.getRight() == I(p[9]) && g.getBottom() == I(p[10]) &&
                      g.rectX() == I(p[11]) && g.rectY() == I(p[12]) &&
                      g.rectW() == I(p[13]) && g.rectH() == I(p[14]) &&
                      (int)g.isActive() == I(p[15]) && (int)g.isHoveredFn() == I(p[16]) &&
                      (int)g.isFocused() == I(p[17]) && (int)g.isHoveredOrFocused() == I(p[18]) &&
                      (int)g.narrationPriority() == I(p[19]);
            if (!ok) fail("WIDGET (" + p[1] + "," + p[2] + "," + p[3] + "," + p[4] + ") flags " +
                          p[5] + p[6] + p[7] + p[8]);
        } else if (t == "OVER") {
            ++n;
            auto g = mk(p[1], p[2], p[3], p[4], I(p[5]), I(p[6]), 0, 0);
            if ((int)g.isMouseOver(D(p[7]), D(p[8])) != I(p[9]))
                fail("OVER (" + p[1] + "," + p[2] + "," + p[3] + "," + p[4] + ") a" + p[5] + "v" + p[6]);
        } else if (t == "CLICK") {
            ++n;
            auto g = mk(p[1], p[2], p[3], p[4], I(p[5]), I(p[6]), 0, 0);
            int btn = I(p[7]);
            double mx = D(p[8]), my = D(p[9]);
            bool ok = (int)g.isActive() == I(p[10]) && (int)g.isValidClickButton(btn) == I(p[11]) &&
                      (int)g.isMouseOver(mx, my) == I(p[12]) &&
                      (int)g.wouldAcceptClick(btn, mx, my) == I(p[13]);
            if (!ok) fail("CLICK (" + p[1] + "," + p[2] + "," + p[3] + "," + p[4] + ") a" + p[5] +
                          " btn" + p[7]);
        }
    }
    std::cout << "WidgetGeometry checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
