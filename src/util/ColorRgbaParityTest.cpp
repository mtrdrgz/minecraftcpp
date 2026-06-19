// Parity test for net.minecraft.util.ColorRGBA. Ground truth:
// tools/ColorRgbaParity.java vs the real class. Verifies constructor + rgba()
// accessor round-trip, toString() (8-digit lowercase hex), record equality, and
// the packed channel pack/unpack of the held int, bit-exact.
//
//   color_rgba_parity --cases mcpp/build/color_rgba.tsv

#include "ColorRGBA.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace util = mc::util;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Inputs are emitted as Java ints (may exceed 2^31 as unsigned hex literals in
// the source, but printed signed decimal): parse via long long then narrow.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: color_rgba_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqS = [&](const std::string& got, const std::string& exp, const std::string& l) {
        if (got != exp) fail(l + " got=" + got);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "RGBA") {
            // constructor + rgba() accessor round-trip
            util::ColorRGBA c(i(p[1]));
            eqI(c.rgbaValue(), p[2], line);
        } else if (t == "STR") {
            util::ColorRGBA c(i(p[1]));
            eqS(c.toString(), p[2], line);
        } else if (t == "EQ") {
            util::ColorRGBA a(i(p[1]));
            util::ColorRGBA b(i(p[1]));
            eqI((a == b) ? 1 : 0, p[2], line);
        } else if (t == "NEQ") {
            util::ColorRGBA a(i(p[1]));
            util::ColorRGBA b(i(p[2]));
            eqI((a == b) ? 1 : 0, p[3], line);
        } else if (t == "ALPHA") {
            util::ColorRGBA c(i(p[1]));
            eqI(c.alpha(), p[2], line);
        } else if (t == "RED") {
            util::ColorRGBA c(i(p[1]));
            eqI(c.red(), p[2], line);
        } else if (t == "GREEN") {
            util::ColorRGBA c(i(p[1]));
            eqI(c.green(), p[2], line);
        } else if (t == "BLUE") {
            util::ColorRGBA c(i(p[1]));
            eqI(c.blue(), p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ColorRGBA cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
