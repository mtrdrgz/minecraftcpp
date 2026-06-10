// Parity gate for mc::client::calculateScale (1:1 port of
// com.mojang.blaze3d.platform.Window#calculateScale, MC 26.1.2).
//
// Reads the TSV produced by GuiScaleParity.java and recomputes each row with
// the engine header, comparing the integer result exactly.
//
//   row: SCALE\t<w>\t<h>\t<maxScale>\t<enforceUnicode 0|1>\t<result>
//
// Usage: gui_scale_parity --cases <path-to-gui_scale.tsv>

#include "../../src/client/GuiScale.h"

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) {
        out.push_back(cur);
    }
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::cerr << "usage: gui_scale_parity --cases <gui_scale.tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open cases file: " << casesPath << "\n";
        return 2;
    }

    long long total = 0;
    long long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = splitTabs(line);
        if (f.empty()) continue;

        if (f[0] == "SCALE") {
            // SCALE  w  h  maxScale  enforceUnicode  result
            if (f.size() < 6) continue;
            int w = (int)std::stoll(f[1]);
            int h = (int)std::stoll(f[2]);
            int maxScale = (int)std::stoll(f[3]);
            bool uni = std::stoll(f[4]) != 0;
            int expected = (int)std::stoll(f[5]);

            int got = mc::client::calculateScale(w, h, maxScale, uni);
            ++total;
            if (got != expected) {
                ++mismatches;
                if (mismatches <= 20) {
                    std::cerr << "MISMATCH SCALE w=" << w << " h=" << h
                              << " maxScale=" << maxScale << " uni=" << (uni ? 1 : 0)
                              << " expected=" << expected << " got=" << got << "\n";
                }
            }
        } else {
            std::cerr << "unknown tag: " << f[0] << "\n";
        }
    }

    std::cout << "GuiScale cases=" << total << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
