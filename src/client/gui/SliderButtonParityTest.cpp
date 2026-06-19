// Parity gate for mc::gui::SliderButton vs the real net.minecraft.client.gui.components.
// AbstractSliderButton value/mouse/handle/keyboard math. Doubles arrive as doubleToRawLongBits,
// reconstructed + compared bit-for-bit; handle X compared as int.
//
//   slider_button_parity --cases mcpp/build/slider_button.tsv

#include "SliderButton.h"

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
std::uint64_t bits(double v) { return std::bit_cast<std::uint64_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: slider_button_parity --cases <tsv>\n"; return 2; }
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
        gui::SliderButton s;
        s.x = I(p[1]); s.y = 0; s.width = I(p[2]); s.height = 20;
        if (t == "SETVAL") {
            ++n;
            s.setValue(D(p[3]));
            if (bits(s.value) != (std::uint64_t)L(p[4]) || s.handleX() != I(p[5]))
                fail("SETVAL x=" + p[1] + " w=" + p[2] + " nv=" + p[3]);
        } else if (t == "MOUSE") {
            ++n;
            s.value = 0.0;
            s.setValueFromMouse(D(p[3]));
            if (bits(s.value) != (std::uint64_t)L(p[4]) || s.handleX() != I(p[5]))
                fail("MOUSE x=" + p[1] + " w=" + p[2] + " mx=" + p[3]);
        } else if (t == "KEY") {
            ++n;
            s.value = D(p[3]);
            bool left = I(p[4]) == 1;
            s.keyStep(left);
            if (bits(s.value) != (std::uint64_t)L(p[5]) || s.handleX() != I(p[6]))
                fail("KEY x=" + p[1] + " w=" + p[2] + " iv=" + p[3] + " left=" + p[4]);
        }
    }
    std::cout << "SliderButton checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
