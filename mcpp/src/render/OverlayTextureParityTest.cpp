// Parity test for mcpp/src/render/OverlayTextureMath.h — the pure static math of
// net.minecraft.client.renderer.texture.OverlayTexture (Minecraft 26.1.2).
//
// Ground truth: tools/OverlayTextureParity.java driving the REAL net.minecraft
// OverlayTexture (no body replicated Java-side). Floats arrive as raw IEEE-754 bits and
// every result is compared as an exact 32-bit int.
//
//   overlay_texture_parity --cases mcpp/build/overlay_texture.tsv

#include "OverlayTextureMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ot = mc::render::overlaytexture;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16)));
}
// Java ints are signed 32-bit and the tool prints them as signed decimal (possibly
// negative, possibly INT_MIN). Parse as long long then narrow to int32.
int parseInt(const std::string& s) {
    return static_cast<int>(static_cast<int32_t>(std::stoll(s)));
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: overlay_texture_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, mismatches = 0;
    auto fail = [&](const std::string& tag, const std::string& detail, int expect, int got) {
        ++mismatches;
        if (mismatches <= 20)
            std::cerr << tag << " " << detail << " expect=" << expect << " got=" << got << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "U") {
            // U progressBits(f) | out(int)
            if (p.size() < 3) continue;
            float prog = bf(p[1]);
            int expect = parseInt(p[2]);
            int got = ot::u(prog);
            ++cases;
            if (got != expect) fail(tag, "prog=" + p[1], expect, got);
        } else if (tag == "V") {
            // V hurt(0|1) | out(int)
            if (p.size() < 3) continue;
            bool hurt = parseInt(p[1]) != 0;
            int expect = parseInt(p[2]);
            int got = ot::v(hurt);
            ++cases;
            if (got != expect) fail(tag, "hurt=" + p[1], expect, got);
        } else if (tag == "PACKII") {
            // PACKII u(int) v(int) | out(int)
            if (p.size() < 4) continue;
            int uu = parseInt(p[1]);
            int vv = parseInt(p[2]);
            int expect = parseInt(p[3]);
            int got = ot::pack(uu, vv);
            ++cases;
            if (got != expect) fail(tag, "u=" + p[1] + " v=" + p[2], expect, got);
        } else if (tag == "PACKFB") {
            // PACKFB progressBits(f) red(0|1) | out(int)
            if (p.size() < 4) continue;
            float prog = bf(p[1]);
            bool red = parseInt(p[2]) != 0;
            int expect = parseInt(p[3]);
            int got = ot::pack(prog, red);
            ++cases;
            if (got != expect) fail(tag, "prog=" + p[1] + " red=" + p[2], expect, got);
        } else if (tag == "CONST") {
            // CONST name | out(int)
            if (p.size() < 3) continue;
            const std::string& name = p[1];
            int expect = parseInt(p[2]);
            int got = 0;
            if (name == "NO_WHITE_U") got = ot::NO_WHITE_U;
            else if (name == "RED_OVERLAY_V") got = ot::RED_OVERLAY_V;
            else if (name == "WHITE_OVERLAY_V") got = ot::WHITE_OVERLAY_V;
            else if (name == "NO_OVERLAY") got = ot::NO_OVERLAY;
            else continue;
            ++cases;
            if (got != expect) fail(tag, "name=" + name, expect, got);
        }
        // unknown tags ignored
    }

    std::cout << "OverlayTexture checks=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
