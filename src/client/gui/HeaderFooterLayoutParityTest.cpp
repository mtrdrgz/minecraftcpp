// Parity gate for mc::gui::headerAndFooterArrange vs the real
// net.minecraft.client.gui.layouts.HeaderAndFooterLayout.arrangeElements. Pure integer compare of the
// header/footer/content child positions + layout heights. Scenarios mirror HeaderFooterLayoutParity.java.
//
//   header_footer_layout_parity --cases mcpp/build/header_footer_layout.tsv

#include "HeaderAndFooterLayout.h"

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

// Must match HeaderFooterLayoutParity.java exactly.
const std::vector<std::vector<int>> CFG = {
    {320, 240, 33, 33}, {640, 480, 48, 60}, {200, 150, 33, 33}, {400, 300, 20, 20}, {500, 400, 33, 33}};
const std::vector<std::vector<int>> HDR = {
    {200, 9}, {300, 20, 100, 40}, {180, 33}, {}, {250, 9}};
const std::vector<std::vector<int>> FTR = {
    {150, 20}, {200, 30}, {180, 33}, {100, 10}, {250, 20, 60, 9}};
const std::vector<std::vector<int>> CON = {
    {220, 40}, {400, 100, 380, 150}, {190, 120}, {220, 40, 50, 200}, {220, 260}};

// Default child layout settings: align(0.5, 0.5), padding 0.
std::vector<gui::GridChild> spacers(const std::vector<int>& flat) {
    std::vector<gui::GridChild> v;
    for (size_t i = 0; i + 1 < flat.size(); i += 2) {
        gui::GridChild c;
        c.childW = flat[i];
        c.childH = flat[i + 1];
        c.alignX = 0.5f;
        c.alignY = 0.5f;
        v.push_back(c);
    }
    return v;
}

struct Built {
    std::vector<gui::GridChild> header, footer, contents;
    gui::HeaderFooterResult r;
};

Built build(int s) {
    Built b;
    b.header = spacers(HDR[s]);
    b.footer = spacers(FTR[s]);
    b.contents = spacers(CON[s]);
    const auto& c = CFG[s];
    b.r = gui::headerAndFooterArrange(c[0], c[1], c[2], c[3], b.header, b.footer, b.contents);
    return b;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: header_footer_layout_parity --cases <tsv>\n"; return 2; }
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
        int s = I(p[1]);
        auto b = build(s);
        if (t == "LAYOUT") {
            ++n;
            bool ok = b.r.headerHeight == I(p[6]) && b.r.footerHeight == I(p[7]) &&
                      b.r.contentHeight == I(p[8]);
            if (!ok) fail("LAYOUT s=" + p[1]);
        } else if (t == "HPOS" || t == "FPOS" || t == "CPOS") {
            ++n;
            int i = I(p[2]);
            const auto& vec = (t == "HPOS") ? b.header : (t == "FPOS") ? b.footer : b.contents;
            if (vec[i].outX != I(p[3]) || vec[i].outY != I(p[4]))
                fail(t + " s=" + p[1] + " i=" + p[2] + " want(" + p[3] + "," + p[4] + ") got(" +
                     std::to_string(vec[i].outX) + "," + std::to_string(vec[i].outY) + ")");
        }
    }
    std::cout << "HeaderFooterLayout checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
