// Byte-exact parity for the GUI layout positioning math vs the REAL
// net.minecraft.client.gui.layouts.{FrameLayout, SpacerElement, LayoutSettings,
// AbstractChildWrapper} (ground truth: mcpp/tools/FrameLayoutParity.java).
//
//   frame_layout_parity [--cases mcpp/build/frame_layout.tsv]
//
// Prints:  FrameLayout cases=<N> mismatches=<M>   (nonzero exit iff M>0)
#include "FrameLayout.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::gui::FrameLayout;
using mc::gui::LayoutSettings;
using mc::gui::SpacerElement;

namespace {
float bitsToF(const std::string& tok) {
    int32_t bits = static_cast<int32_t>(std::stoll(tok));
    float f;
    std::memcpy(&f, &bits, 4);
    return f;
}
std::vector<std::string> split(const std::string& s) {
    std::vector<std::string> v;
    std::string cur;
    for (char c : s) { if (c == '\t') { v.push_back(cur); cur.clear(); } else cur.push_back(c); }
    v.push_back(cur);
    return v;
}
int I(const std::string& s) { return std::stoi(s); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/frame_layout.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mism = 0;
    auto eq = [&](const char* what, const std::string& name, int got, int want) {
        if (got != want) { ++mism; std::cerr << what << "-MISMATCH " << name << " got " << got << " want " << want << "\n"; }
    };

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> t = split(line);
        const std::string& tag = t[0];

        if (tag == "FRAME") {
            // x y minW minH cw ch padL padT padR padB axBits ayBits outX outY outW outH
            if (t.size() != 17) { ++mism; std::cerr << "bad FRAME row\n"; continue; }
            int x = I(t[1]), y = I(t[2]), minW = I(t[3]), minH = I(t[4]), cw = I(t[5]), ch = I(t[6]);
            int padL = I(t[7]), padT = I(t[8]), padR = I(t[9]), padB = I(t[10]);
            float ax = bitsToF(t[11]), ay = bitsToF(t[12]);
            FrameLayout frame(x, y, minW, minH);
            SpacerElement child(cw, ch);
            LayoutSettings s = FrameLayout::newChildLayoutSettings().padding(padL, padT, padR, padB).align(ax, ay);
            frame.addChild(&child, s);
            frame.arrangeElements();
            ++cases;
            std::string nm = "FRAME[" + line.substr(6, 24) + "]";
            eq("FRAME.X", nm, child.getX(), I(t[13]));
            eq("FRAME.Y", nm, child.getY(), I(t[14]));
            eq("FRAME.W", nm, frame.getWidth(), I(t[15]));
            eq("FRAME.H", nm, frame.getHeight(), I(t[16]));
        } else if (tag == "ALIGND") {
            // pos length widgetLen alignBits out
            if (t.size() != 6) { ++mism; std::cerr << "bad ALIGND row\n"; continue; }
            int got = FrameLayout::alignInDimension(I(t[1]), I(t[2]), I(t[3]), bitsToF(t[4]));
            ++cases;
            eq("ALIGND", "ALIGND[" + line.substr(7, 24) + "]", got, I(t[5]));
        } else if (tag == "MULTI3") {
            // x y minW minH w0 h0 w1 h1 w2 h2 axBits ayBits x0 y0 x1 y1 x2 y2 outW outH (21 fields)
            if (t.size() != 21) { ++mism; std::cerr << "bad MULTI3 row\n"; continue; }
            int x = I(t[1]), y = I(t[2]), minW = I(t[3]), minH = I(t[4]);
            float ax = bitsToF(t[11]), ay = bitsToF(t[12]);
            FrameLayout frame(x, y, minW, minH);
            SpacerElement c0(I(t[5]), I(t[6])), c1(I(t[7]), I(t[8])), c2(I(t[9]), I(t[10]));
            frame.addChild(&c0, FrameLayout::newChildLayoutSettings().align(ax, ay));
            frame.addChild(&c1, FrameLayout::newChildLayoutSettings().align(ax, ay));
            frame.addChild(&c2, FrameLayout::newChildLayoutSettings().align(ax, ay));
            frame.arrangeElements();
            ++cases;
            std::string nm = "MULTI3[" + line.substr(7, 20) + "]";
            eq("M3.x0", nm, c0.getX(), I(t[13])); eq("M3.y0", nm, c0.getY(), I(t[14]));
            eq("M3.x1", nm, c1.getX(), I(t[15])); eq("M3.y1", nm, c1.getY(), I(t[16]));
            eq("M3.x2", nm, c2.getX(), I(t[17])); eq("M3.y2", nm, c2.getY(), I(t[18]));
            eq("M3.W", nm, frame.getWidth(), I(t[19])); eq("M3.H", nm, frame.getHeight(), I(t[20]));
        }
    }

    std::cout << "FrameLayout cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
