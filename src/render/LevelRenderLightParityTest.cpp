// Parity gate for mc::render::levelrender::getLightCoords (LevelRenderer.getLightCoords) — the
// per-block packed light-coords. Re-computes from the GT's resolved (emissive, packedBrightness,
// lightEmission) inputs and compares the int output.
//
//   level_render_light_parity --cases mcpp/build/level_render_light.tsv

#include "LevelRenderLight.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace lr = mc::render::levelrender;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: level_render_light_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, bad = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "LC") continue;
        ++n;
        bool emissive = std::stoi(p[1]) != 0;
        int packedBrightness = std::stoi(p[2]);
        int lightEmission = std::stoi(p[3]);
        int exp = std::stoi(p[4]);
        int got = lr::getLightCoords(emissive, packedBrightness, lightEmission);
        if (got != exp) {
            ++bad;
            if (shown++ < 20)
                std::cerr << "MISMATCH emissive=" << emissive << " pb=" << packedBrightness
                          << " le=" << lightEmission << " got=" << got << " exp=" << exp << "\n";
        }
    }
    std::cout << "LevelRenderLight cases=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
