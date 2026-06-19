// Parity test for net.minecraft.world.level.LightLayer (Minecraft 26.1.2).
//
// Ground truth: mcpp/tools/LightLayerParity.java (calls the REAL enum).
// This test reconstructs the same facts from the ported enum in
// world/level/LightLayer.h and compares them exactly: the constant count, and
// the (ordinal, name) of every constant (and its valueOf round-trip).
//
//   light_layer_parity --cases mcpp/build/light_layer.tsv

#include "LightLayer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using mc::world::level::LightLayer;
using mc::world::level::lightLayerName;
using mc::world::level::lightLayerOrdinal;
using mc::world::level::LIGHT_LAYER_COUNT;
using mc::world::level::LIGHT_LAYER_VALUES;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}
int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// values()-indexed lookup of the ported constant (== ordinal index).
bool constantAt(int ordinal, LightLayer& out) {
    if (ordinal < 0 || ordinal >= LIGHT_LAYER_COUNT) return false;
    out = LIGHT_LAYER_VALUES[static_cast<std::size_t>(ordinal)];
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: light_layer_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "CNT") {
            ++total;
            if (toI(p[1]) != LIGHT_LAYER_COUNT) fail(line, "CNT");
        } else if (tag == "ORD" || tag == "VAL") {
            ++total;
            // ORD/VAL ordinal name
            int ordinal = toI(p[1]);
            const std::string& name = p[2];
            LightLayer c;
            if (!constantAt(ordinal, c)) { fail(line, tag + " ordinal out of range"); continue; }
            if (lightLayerOrdinal(c) != ordinal) { fail(line, tag + " ordinal"); continue; }
            if (std::string(lightLayerName(c)) != name) {
                fail(line, tag + " name=" + std::string(lightLayerName(c)));
                continue;
            }
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "LightLayer cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
