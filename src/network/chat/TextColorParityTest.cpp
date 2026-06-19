// Parity gate for mc::chat::TextColor vs the real net.minecraft.network.chat.TextColor. Inputs/strings
// arrive base64 (UTF-8). Compares parse error-ness + value + serialize, RGB masking, fromLegacyFormat.
//
//   text_color_parity --cases mcpp/build/text_color.tsv

#include "TextColor.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace chat = mc::chat;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int I(const std::string& s) { return std::stoi(s); }

std::string b64decode(const std::string& in) {
    static const std::string T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int val = 0, bits = -8;
    std::string out;
    for (char c : in) {
        if (c == '=') break;
        auto pos = T.find(c);
        if (pos == std::string::npos) continue;
        val = (val << 6) + static_cast<int>(pos);
        bits += 6;
        if (bits >= 0) { out.push_back(static_cast<char>((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: text_color_parity --cases <tsv>\n"; return 2; }
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
        if (t == "PARSE") {
            ++n;
            std::string input = b64decode(p[1]);
            int wantErr = I(p[2]);
            auto r = chat::parseColor(input);
            if ((int)r.error != wantErr) { fail("PARSE err [" + input + "]"); continue; }
            if (!r.error) {
                std::string wantSer = b64decode(p[4]);
                if (r.color.value != I(p[3]) || r.color.serialize() != wantSer)
                    fail("PARSE val [" + input + "] got " + std::to_string(r.color.value) + "/" +
                         r.color.serialize() + " want " + p[3] + "/" + wantSer);
            }
        } else if (t == "RGB") {
            ++n;
            auto tc = chat::fromRgb(I(p[1]));
            std::string wantSer = b64decode(p[3]);
            if (tc.value != I(p[2]) || tc.serialize() != wantSer)
                fail("RGB " + p[1] + " got " + std::to_string(tc.value) + "/" + tc.serialize());
        } else if (t == "LEGACY") {
            ++n;
            int ord = I(p[1]);
            const auto& f = mc::chat_formatting::VALUES[ord];
            auto tc = chat::fromLegacyFormat(f);
            int wantNull = I(p[2]);
            if ((int)!tc.has_value() != wantNull) { fail("LEGACY null ord=" + p[1]); continue; }
            if (tc) {
                std::string wantSer = b64decode(p[4]);
                if (tc->value != I(p[3]) || tc->serialize() != wantSer)
                    fail("LEGACY ord=" + p[1] + " got " + std::to_string(tc->value) + "/" + tc->serialize());
            }
        }
    }
    std::cout << "TextColor checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
