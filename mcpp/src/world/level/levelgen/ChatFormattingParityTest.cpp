// Parity test for net.minecraft.ChatFormatting. Ground truth:
// tools/ChatFormattingParity.java vs the real enum's public accessors + static
// lookups.
//
// Verifies, bit-for-bit, every ChatFormatting constant's
//   (ordinal, name, char code, isFormat, isColor, id, hasColor, color, serialized)
// and the full getByCode(char) / getById(int) / getByName(String) lookups across
// a battery of inputs (every code char + misses; ids straddling [0,15] incl. the
// int extremes; names exact/cased/cleaned/missing).
//
//   chat_formatting_parity --cases mcpp/build/chat_formatting.tsv

#include "../../../ChatFormatting.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cf = mc::chat_formatting;

namespace {
// Split on '\t'. A field may be empty or contain spaces (names do); only the tab
// is a delimiter. Trailing empty fields after the last tab are not produced by
// getline, but every row here keeps a non-name field last, so that's fine.
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints printed as signed decimal; parse via long long then narrow.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: chat_formatting_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 60) std::cerr << "MISMATCH " << l << "\n";
    };
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
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "CF") {
            // CF <ordinal> <name> <code> <isFormat> <isColor> <id> <hasColor> <color> <serialized>
            const int ordinal = i(p[1]);
            if (ordinal < 0 || ordinal >= cf::COUNT) {
                fail(line + " ordinal-out-of-range");
                continue;
            }
            const mc::ChatFormattingData& f =
                cf::VALUES[static_cast<std::size_t>(ordinal)];
            // name() == our stored `name`
            eqS(std::string(f.name), p[2], line);
            // getChar() as decimal char code
            eqI(static_cast<int>(static_cast<unsigned char>(cf::getChar(f))), p[3], line);
            // isFormat()
            eqI(cf::isFormat(f) ? 1 : 0, p[4], line);
            // isColor()
            eqI(cf::isColor(f) ? 1 : 0, p[5], line);
            // getId()
            eqI(cf::getId(f), p[6], line);
            // getColor() != null
            auto col = cf::getColor(f);
            const bool hasColor = col.has_value();
            eqI(hasColor ? 1 : 0, p[7], line);
            // color value (0 when null, gated by hasColor)
            eqI(hasColor ? *col : 0, p[8], line);
            // getSerializedName() (== getName())
            eqS(cf::getSerializedName(f), p[9], line);
        } else if (t == "BYCODE") {
            // BYCODE <code> <resultOrdinal>; -1 when null.
            const int code = i(p[1]);
            const mc::ChatFormattingData* r = cf::getByCode(static_cast<char>(code));
            const int got = (r == nullptr) ? -1 : r->ordinal;
            eqI(got, p[2], line);
        } else if (t == "BYID") {
            // BYID <id> <resultOrdinal>; -1 when null.
            const int id = i(p[1]);
            const mc::ChatFormattingData* r = cf::getById(id);
            const int got = (r == nullptr) ? -1 : r->ordinal;
            eqI(got, p[2], line);
        } else if (t == "BYNAME") {
            // BYNAME <name> <resultOrdinal>; -1 when null. <name> may be empty/spaced.
            // After split, p[1] is the (possibly empty) name; result is the last field.
            // Guard: an empty name field yields p of size 3 with p[1] == "".
            std::string name = (p.size() >= 3) ? p[1] : std::string();
            const std::string& expected = p.back();
            const mc::ChatFormattingData* r = cf::getByName(name);
            const int got = (r == nullptr) ? -1 : r->ordinal;
            eqI(got, expected, line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ChatFormatting cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
