// Parity gate for mc::gui::gridArrange (GridLayout.arrangeElements) vs the real GridLayout. Rebuilds
// each case (children with row/col/span/size/padding/align), arranges, and checks every child's
// computed (x,y).
//
//   grid_layout_parity --cases mcpp/build/grid_layout.tsv

#include "GridLayout.h"

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
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: grid_layout_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    int gx = 0, gy = 0, rs = 0, cs = 0;
    int fx = 0, fy = 0, fminW = 0, fminH = 0;
    std::vector<gui::GridChild> children;
    std::vector<std::pair<int, int>> expected;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "LIN") {
            // LIN <horizontal> <x> <y> <spacing> <count>
            fx = std::stoi(p[2]); fy = std::stoi(p[3]); rs = std::stoi(p[4]);  // reuse fx/fy + rs=spacing
            cs = std::stoi(p[1]);  // reuse cs = horizontal flag
            children.clear(); expected.clear();
        } else if (t == "LCH") {
            gui::GridChild c;
            c.childW = std::stoi(p[1]); c.childH = std::stoi(p[2]);
            c.padL = std::stoi(p[3]); c.padT = std::stoi(p[4]); c.padR = std::stoi(p[5]); c.padB = std::stoi(p[6]);
            c.alignX = bf(p[7]); c.alignY = bf(p[8]);
            children.push_back(c);
        } else if (t == "LPOS") {
            expected.push_back({std::stoi(p[1]), std::stoi(p[2])});
        } else if (t == "LEND") {
            ++cases;
            gui::Orientation o = (cs != 0) ? gui::Orientation::HORIZONTAL : gui::Orientation::VERTICAL;
            gui::linearArrange(o, fx, fy, rs, children);
            for (size_t i = 0; i < children.size() && i < expected.size(); ++i) {
                ++checks;
                if (children[i].outX != expected[i].first || children[i].outY != expected[i].second)
                    fail("LIN case " + std::to_string(cases) + " child " + std::to_string(i) +
                         " got=(" + std::to_string(children[i].outX) + "," + std::to_string(children[i].outY) +
                         ") exp=(" + std::to_string(expected[i].first) + "," + std::to_string(expected[i].second) + ")");
            }
        } else if (t == "FRAME") {
            fx = std::stoi(p[1]); fy = std::stoi(p[2]); fminW = std::stoi(p[3]); fminH = std::stoi(p[4]);
            children.clear(); expected.clear();
        } else if (t == "FCH") {
            gui::GridChild c;
            c.childW = std::stoi(p[1]); c.childH = std::stoi(p[2]);
            c.padL = std::stoi(p[3]); c.padT = std::stoi(p[4]); c.padR = std::stoi(p[5]); c.padB = std::stoi(p[6]);
            c.alignX = bf(p[7]); c.alignY = bf(p[8]);
            children.push_back(c);
        } else if (t == "FPOS") {
            expected.push_back({std::stoi(p[1]), std::stoi(p[2])});
        } else if (t == "FEND") {
            ++cases;
            gui::frameArrange(fx, fy, fminW, fminH, children);
            for (size_t i = 0; i < children.size() && i < expected.size(); ++i) {
                ++checks;
                if (children[i].outX != expected[i].first || children[i].outY != expected[i].second)
                    fail("FRAME case " + std::to_string(cases) + " child " + std::to_string(i) +
                         " got=(" + std::to_string(children[i].outX) + "," + std::to_string(children[i].outY) +
                         ") exp=(" + std::to_string(expected[i].first) + "," + std::to_string(expected[i].second) + ")");
            }
        } else if (t == "CASE") {
            gx = std::stoi(p[1]); gy = std::stoi(p[2]); rs = std::stoi(p[3]); cs = std::stoi(p[4]);
            children.clear();
            expected.clear();
        } else if (t == "CH") {
            gui::GridChild c;
            c.row = std::stoi(p[1]); c.col = std::stoi(p[2]); c.occRows = std::stoi(p[3]); c.occCols = std::stoi(p[4]);
            c.childW = std::stoi(p[5]); c.childH = std::stoi(p[6]);
            c.padL = std::stoi(p[7]); c.padT = std::stoi(p[8]); c.padR = std::stoi(p[9]); c.padB = std::stoi(p[10]);
            c.alignX = bf(p[11]); c.alignY = bf(p[12]);
            children.push_back(c);
        } else if (t == "POS") {
            expected.push_back({std::stoi(p[1]), std::stoi(p[2])});
        } else if (t == "END") {
            ++cases;
            gui::gridArrange(gx, gy, rs, cs, children);
            for (size_t i = 0; i < children.size() && i < expected.size(); ++i) {
                ++checks;
                if (children[i].outX != expected[i].first || children[i].outY != expected[i].second)
                    fail("case " + std::to_string(cases) + " child " + std::to_string(i) +
                         " got=(" + std::to_string(children[i].outX) + "," + std::to_string(children[i].outY) +
                         ") exp=(" + std::to_string(expected[i].first) + "," + std::to_string(expected[i].second) + ")");
            }
        }
    }
    std::cout << "GridLayout cases=" << cases << " checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
