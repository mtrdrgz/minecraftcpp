// Parity gate for the PURE surface of mc::chat::Style vs the real net.minecraft.network.chat.Style.
// The Java ground truth (tools/StyleParity.java) drives the REAL Style through a fixed op stream and
// emits, after every op, an OP row (the instruction) and a STATE row (every observable). This test
// replays the SAME ops on the C++ port and checks each STATE row byte-for-byte.
//
//   style_parity --cases mcpp/build/style.tsv
//
// Parents (referenced by APPLYTO) are rebuilt here identically to the Java side.

#include "Style.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace chat = mc::chat;
namespace cf = mc::chat_formatting;

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

std::optional<bool> tri(const std::string& s) {
    if (s == "T") return true;
    if (s == "F") return false;
    return std::nullopt;  // "N"
}

const mc::ChatFormattingData& CF(int ord) { return cf::VALUES[ord]; }

// Parents identical to StyleParity.parents() in Java.
std::vector<chat::Style> buildParents() {
    using chat::Style;
    using chat::TextColor;
    std::vector<Style> p(4);
    p[0] = Style{}
        .withBold(true)
        .withItalic(true)
        .withUnderlined(true)
        .withStrikethrough(false)
        .withObfuscated(false)
        .withColor(std::optional<TextColor>(chat::fromRgb(0x654321)));
    p[1] = Style{}
        .withBold(false)
        .withObfuscated(true)
        .withColor(&CF(11));  // AQUA (ordinal 11 in ChatFormatting VALUES)
    p[2] = Style{}
        .withBold(false)
        .withItalic(false)
        .withUnderlined(false)
        .withStrikethrough(false)
        .withObfuscated(false);
    p[3] = Style{};  // EMPTY
    return p;
}

// Parse a comma-separated ordinal list ("1,2,3" or "").
std::vector<const mc::ChatFormattingData*> parseFormats(const std::string& s) {
    std::vector<const mc::ChatFormattingData*> out;
    if (s.empty()) return out;
    std::string it;
    std::istringstream ss(s);
    while (std::getline(ss, it, ',')) out.push_back(&CF(std::stoi(it)));
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: style_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    std::vector<chat::Style> parents = buildParents();

    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    chat::Style cur;  // EMPTY
    std::string pendingOp;  // human label of the op that produced the next STATE

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "OP") {
            const std::string& name = p[1];
            pendingOp = line;
            if (name == "WBOLD") cur = cur.withBold(tri(p[2]));
            else if (name == "WITAL") cur = cur.withItalic(tri(p[2]));
            else if (name == "WUNDER") cur = cur.withUnderlined(tri(p[2]));
            else if (name == "WSTRIKE") cur = cur.withStrikethrough(tri(p[2]));
            else if (name == "WOBF") cur = cur.withObfuscated(tri(p[2]));
            else if (name == "WCOL_TC") cur = cur.withColor(std::optional<chat::TextColor>(chat::fromRgb(I(p[2]))));
            else if (name == "WCOL_TCNULL") cur = cur.withColor(std::optional<chat::TextColor>(std::nullopt));
            else if (name == "WCOL_CF") cur = cur.withColor(&CF(I(p[2])));
            else if (name == "WCOL_CFNULL") cur = cur.withColor(static_cast<const mc::ChatFormattingData*>(nullptr));
            else if (name == "AFMT") cur = cur.applyFormat(CF(I(p[2])));
            else if (name == "ALEG") cur = cur.applyLegacyFormat(CF(I(p[2])));
            else if (name == "AFMTS") {
                auto fs = parseFormats(p.size() > 2 ? p[2] : std::string());
                cur = cur.applyFormatsRange(fs.begin(), fs.end());
            }
            else if (name == "APPLYTO") cur = cur.applyTo(parents[I(p[2])]);
            else if (name == "RESET_STYLE") cur = chat::Style{};
            else { std::cerr << "unknown op " << name << "\n"; return 2; }
        } else if (tag == "STATE") {
            ++n;
            int wBold = I(p[1]), wItal = I(p[2]), wUnder = I(p[3]), wStrike = I(p[4]), wObf = I(p[5]);
            int wHasColor = I(p[6]); int wVal = I(p[7]);
            // p[8] is base64(serialize()); for an absent color it is base64("") == "", which getline
            // drops as a trailing empty field, so it may be missing.
            std::string wSer = (p.size() > 8) ? b64decode(p[8]) : std::string();

            int gBold = cur.isBold() ? 1 : 0;
            int gItal = cur.isItalic() ? 1 : 0;
            int gUnder = cur.isUnderlined() ? 1 : 0;
            int gStrike = cur.isStrikethrough() ? 1 : 0;
            int gObf = cur.isObfuscated() ? 1 : 0;
            const auto& col = cur.getColor();
            int gHasColor = col ? 1 : 0;
            int gVal = col ? col->value : 0;
            std::string gSer = col ? col->serialize() : "";

            if (gBold != wBold || gItal != wItal || gUnder != wUnder || gStrike != wStrike ||
                gObf != wObf || gHasColor != wHasColor || gVal != wVal || gSer != wSer) {
                std::ostringstream m;
                m << "after [" << pendingOp << "] got "
                  << gBold << gItal << gUnder << gStrike << gObf
                  << " hasColor=" << gHasColor << " val=" << gVal << " ser=" << gSer
                  << " want "
                  << wBold << wItal << wUnder << wStrike << wObf
                  << " hasColor=" << wHasColor << " val=" << wVal << " ser=" << wSer;
                fail(m.str());
            }
        }
    }

    std::cout << "Style checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
