// Parity gate for mc::gui::SelectionList vs the real net.minecraft.client.gui.components.
// AbstractSelectionList row/layout geometry. Mirrors the Java build sequence (updateSizeAndPosition,
// addEntry* at scroll 0, setScrollAmount) then compares all geometry. Mouse doubles arrive as
// doubleToRawLongBits.
//
//   selection_list_parity --cases mcpp/build/selection_list.tsv

#include "SelectionList.h"

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

// CFG/HEIGHTS/SCROLLS must match SelectionListParity.java exactly.
const std::vector<std::vector<int>> CFG = {
    {0, 0, 300, 200, 20}, {0, 0, 300, 100, 25}, {20, 10, 400, 120, 18},
    {-30, -20, 250, 80, 22}, {5, 5, 320, 150, 20}};
const std::vector<std::vector<int>> HEIGHTS = {
    {20, 20, 20, 20, 20}, {25, 25, 25, 25, 25, 25, 25, 25},
    {18, 18, 18, 18, 18, 18, 18, 18, 18, 18}, {22, 30, 15, 40, 22}, {}};
const double SCROLLS[5] = {0.0, 50.0, 30.0, 53.0, 0.0};

gui::SelectionList build(int s) {
    const auto& c = CFG[s];
    gui::SelectionList l;
    l.x = 0; l.y = c[1]; l.width = c[2]; l.height = c[3]; l.defaultEntryHeight = c[4];
    l.updateSizeAndPosition(c[2], c[3], c[0], c[1]);
    for (int h : HEIGHTS[s]) l.addEntry(h);
    l.setScrollAmount(SCROLLS[s]);
    return l;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: selection_list_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    // cfg cols (p[1..7]) identify the scenario; find its index by matching x,y,w,h,deh.
    auto scenario = [&](const std::vector<std::string>& p) -> int {
        for (int s = 0; s < (int)CFG.size(); ++s)
            if (CFG[s][0] == I(p[1]) && CFG[s][1] == I(p[2]) && CFG[s][2] == I(p[3]) &&
                CFG[s][3] == I(p[4]) && CFG[s][4] == I(p[5]))
                return s;
        return -1;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        int s = scenario(p);
        if (s < 0) { fail("unknown scenario " + line); continue; }
        auto l = build(s);
        if (t == "LIST") {
            ++n;
            bool ok = l.contentHeight() == I(p[8]) && l.maxScrollAmount() == I(p[9]) &&
                      l.scrollerHeight() == I(p[10]) && l.scrollBarX() == I(p[11]) &&
                      l.getRowLeft() == I(p[12]) && l.getRowRight() == I(p[13]) &&
                      l.getRowWidth() == I(p[14]) && l.getNextY() == I(p[15]) &&
                      std::bit_cast<std::uint64_t>(l.scrollAmount()) == (std::uint64_t)std::stoll(p[16]);
            if (!ok) fail("LIST s=" + std::to_string(s));
        } else if (t == "ROW") {
            ++n;
            int i = I(p[8]);
            const auto& e = l.children[i];
            bool ok = l.getRowTop(i) == I(p[9]) && l.getRowBottom(i) == I(p[10]) &&
                      e.getX() == I(p[11]) && e.getY() == I(p[12]) && e.getWidth() == I(p[13]) &&
                      e.getHeight() == I(p[14]) && e.getContentX() == I(p[15]) &&
                      e.getContentY() == I(p[16]) && e.getContentHeight() == I(p[17]) &&
                      e.getContentYMiddle() == I(p[18]) && e.getContentBottom() == I(p[19]) &&
                      e.getContentWidth() == I(p[20]) && e.getContentXMiddle() == I(p[21]) &&
                      e.getContentRight() == I(p[22]);
            if (!ok) fail("ROW s=" + std::to_string(s) + " i=" + p[8]);
        } else if (t == "EAT") {
            ++n;
            if (l.getEntryAtPosition(D(p[8]), D(p[9])) != I(p[10]))
                fail("EAT s=" + std::to_string(s));
        }
    }
    std::cout << "SelectionList checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
