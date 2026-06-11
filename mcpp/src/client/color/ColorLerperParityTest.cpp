// Parity test for net.minecraft.client.color.ColorLerper. Ground truth:
// tools/ColorLerperParity.java vs the REAL class (+ its Type enum via reflection).
// Verifies the baked DyeColor palette, the private getModifiedColor helper, each
// Type's precomputed palette, and the public getLerpedColor over a tick battery —
// all bit-exact (packed colors as ints, floats decoded from raw IEEE-754 bits).
//
//   colorlerper_parity --cases mcpp/build/colorlerper.tsv

#include "ColorLerper.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cl = mc::client::color;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int   i(const std::string& s)  { return std::stoi(s); }
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }

const cl::Type& typeByName(const std::string& n) {
    return n == "SHEEP" ? cl::SHEEP() : cl::MUSIC_NOTE();
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: colorlerper_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "DYE") {
            // DYE <ordinal> <getTextureDiffuseColor>
            eqI(cl::getTextureDiffuseColor(static_cast<cl::DyeColor>(i(p[1]))), p[2], line);
        } else if (t == "MOD") {
            // MOD <ordinal> <brightnessBits> <getModifiedColor>
            eqI(cl::getModifiedColor(static_cast<cl::DyeColor>(i(p[1])), bf(p[2])), p[3], line);
        } else if (t == "TYPE") {
            // TYPE <name> <colorDuration> <paletteLen>
            const cl::Type& ty = typeByName(p[1]);
            eqI(ty.colorDuration, p[2], line);
            eqI(ty.colorCount, p[3], line);
        } else if (t == "PAL") {
            // PAL <name> <index> <precomputed color>
            const cl::Type& ty = typeByName(p[1]);
            eqI(ty.getColorAt(i(p[2])), p[3], line);
        } else if (t == "LERP") {
            // LERP <name> <tickBits> <getLerpedColor>
            eqI(cl::getLerpedColor(typeByName(p[1]), bf(p[2])), p[3], line);
        } else {
            fail(line + " <unknown-tag>");
        }
    }

    std::cout << "ColorLerper checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
