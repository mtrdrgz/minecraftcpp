// Parity gate for cuboidjson::parseItemTransforms / parseItemTransform (the model "display" block)
// vs the real ItemTransforms/ItemTransform deserializers. Decodes each base64 display block, parses
// it, and compares every context's rotation/translation/scale (raw float bits).
//
//   item_transforms_parity --cases mcpp/build/item_transforms.tsv

#include "CuboidModelJson.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cj = mc::render::model::cuboidjson;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }

std::string base64Decode(const std::string& in) {
    static const std::string T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int dec[256];
    for (int i = 0; i < 256; ++i) dec[i] = -1;
    for (int i = 0; i < 64; ++i) dec[(unsigned char)T[i]] = i;
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || dec[c] == -1) continue;
        val = (val << 6) + dec[c];
        bits += 6;
        if (bits >= 0) { out.push_back(char((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: item_transforms_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, checks = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "IT") continue;
        ++cases;
        cj::json display = cj::json::parse(base64Decode(p[1]));
        cj::ItemTransforms tf = cj::parseItemTransforms(display);
        int col = 2;
        for (int c = 0; c < 9; ++c) {
            const cj::ItemTransform& it = tf.byName[c];
            const float* vals[3] = {it.rot, it.trans, it.scale};
            bool ok = true;
            for (int g = 0; g < 3 && ok; ++g)
                for (int k = 0; k < 3 && ok; ++k)
                    if (fbits(vals[g][k]) != static_cast<uint32_t>(std::stoul(p[col + g * 3 + k], nullptr, 16))) ok = false;
            ++checks;
            if (!ok) fail("case " + std::to_string(cases) + " ctx " + std::to_string(c));
            col += 9;
        }
    }
    std::cout << "ItemTransforms cases=" << cases << " checks=" << checks << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
