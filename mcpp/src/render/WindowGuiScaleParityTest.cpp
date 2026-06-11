// Parity test for the GUI-scale math of com.mojang.blaze3d.platform.Window
// (calculateScale + setGuiScale). Ground truth: tools/WindowGuiScaleParity.java
// (drives the REAL Window via Unsafe.allocateInstance). Bit-exact on the integer
// outputs (the math is integer-valued; setGuiScale's intermediate double only
// affects the integer result, which we compare exactly).
//
//   window_gui_scale_parity --cases mcpp/build/window_gui_scale.tsv

#include "WindowGuiScale.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t pi(const std::string& s) { return static_cast<int32_t>(std::stol(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: window_gui_scale_parity --cases <tsv>\n";
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

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;
        if (t == "SCALE") {
            // SCALE fbW fbH maxScale enforceUnicode -> calculateScale
            mc::render::WindowGuiScale w(pi(p[1]), pi(p[2]));
            int32_t got = w.calculateScale(pi(p[3]), pi(p[4]) != 0);
            int32_t want = pi(p[5]);
            if (got != want) fail(line + " got=" + std::to_string(got));
        } else if (t == "GUI") {
            // GUI fbW fbH guiScale -> guiScaledWidth guiScaledHeight
            mc::render::WindowGuiScale w(pi(p[1]), pi(p[2]));
            w.setGuiScale(pi(p[3]));
            int32_t gw = w.getGuiScaledWidth();
            int32_t gh = w.getGuiScaledHeight();
            int32_t wantW = pi(p[4]);
            int32_t wantH = pi(p[5]);
            if (gw != wantW || gh != wantH)
                fail(line + " gotW=" + std::to_string(gw) + " gotH=" + std::to_string(gh));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "WindowGuiScale checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
