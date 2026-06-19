// Parity gate for mc::gui::ScreenRectangle vs the real net.minecraft.client.gui.navigation.
// ScreenRectangle. Pure integer compare of bounds + overlaps/intersects/encompasses/intersection/
// containsPoint.
//
//   screen_rectangle_parity --cases mcpp/build/screen_rectangle.tsv

#include "ScreenRectangle.h"

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
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: screen_rectangle_parity --cases <tsv>\n"; return 2; }
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
        if (t == "RECT") {
            ++n;
            gui::ScreenRectangle a{I(p[1]), I(p[2]), I(p[3]), I(p[4])};
            if (a.top() != I(p[5]) || a.bottom() != I(p[6]) || a.left() != I(p[7]) || a.right() != I(p[8]))
                fail("RECT bounds");
        } else if (t == "PAIR") {
            ++n;
            gui::ScreenRectangle a{I(p[1]), I(p[2]), I(p[3]), I(p[4])};
            gui::ScreenRectangle b{I(p[5]), I(p[6]), I(p[7]), I(p[8])};
            bool ov = a.overlaps(b), is = a.intersects(b), en = a.encompasses(b);
            gui::ScreenRectangle out;
            bool valid = a.intersection(b, out);
            bool ok = (ov == (I(p[9]) != 0)) && (is == (I(p[10]) != 0)) && (en == (I(p[11]) != 0)) &&
                      (valid == (I(p[12]) != 0));
            if (ok && valid) ok = (out.x == I(p[13]) && out.y == I(p[14]) && out.width == I(p[15]) && out.height == I(p[16]));
            if (!ok) fail("PAIR a=(" + p[1] + "," + p[2] + "," + p[3] + "," + p[4] + ") b=(" + p[5] + "," + p[6] + "," + p[7] + "," + p[8] + ")");
        } else if (t == "PT") {
            ++n;
            gui::ScreenRectangle a{I(p[1]), I(p[2]), I(p[3]), I(p[4])};
            if (a.containsPoint(I(p[5]), I(p[6])) != (I(p[7]) != 0))
                fail("PT (" + p[5] + "," + p[6] + ")");
        }
    }
    std::cout << "ScreenRectangle checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
