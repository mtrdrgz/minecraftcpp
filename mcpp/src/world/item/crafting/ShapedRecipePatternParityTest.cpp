// Byte-exact parity for the PURE coord/dimension/symmetry helpers of
//   net.minecraft.world.item.crafting.ShapedRecipePattern  (Minecraft 26.1.2)
// and net.minecraft.util.Util.isSymmetrical, against the REAL classes
// (ground truth: mcpp/tools/ShapedRecipePatternParity.java).
//
// Replays the TSV produced by the Java GT against the C++ port
// (mcpp/src/world/item/crafting/ShapedRecipePattern.h) and compares:
//   FNE    -> firstNonEmpty(line)            (decimal int)
//   LNE    -> lastNonEmpty(line)             (decimal int)
//   SHRINK -> shrink(pattern)                (row count + each row, base64 bytes)
//   SYM    -> isSymmetrical(w,h,cells)       (bool)
//
// String fields arrive base64(UTF-8); we decode to raw bytes. The pattern rows are
// ASCII so the byte string maps 1:1 onto std::string chars.
//
//   shaped_recipe_pattern_parity --cases mcpp/build/shaped_recipe_pattern.tsv
//
// Prints exactly:  ShapedRecipePattern checks=<N> mismatches=<M>
// Returns nonzero iff M>0. A mismatch is a REAL port bug — fix the port, not the test.

#include "world/item/crafting/ShapedRecipePattern.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace srp = mc::item::crafting;

namespace {

std::string b64decode(const std::string& in) {
    auto val = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    int buf = 0, bits = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        int v = val(c);
        if (v < 0) continue;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { fields.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    fields.push_back(cur);
    return fields;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: shaped_recipe_pattern_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& msg, const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << " | " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "FNE") {
            // FNE <lineB64> <firstNonEmpty>
            std::string s = b64decode(p[1]);
            int exp = std::stoi(p[2]);
            int got = srp::firstNonEmpty(s);
            if (got != exp)
                fail("firstNonEmpty got=" + std::to_string(got) + " exp=" + std::to_string(exp), line);

        } else if (tag == "LNE") {
            // LNE <lineB64> <lastNonEmpty>
            std::string s = b64decode(p[1]);
            int exp = std::stoi(p[2]);
            int got = srp::lastNonEmpty(s);
            if (got != exp)
                fail("lastNonEmpty got=" + std::to_string(got) + " exp=" + std::to_string(exp), line);

        } else if (tag == "SHRINK") {
            // SHRINK <nIn> <inRowB64>... -> <nOut> <outRowB64>...
            size_t idx = 1;
            int nIn = std::stoi(p[idx++]);
            std::vector<std::string> input;
            input.reserve(static_cast<size_t>(nIn));
            for (int i = 0; i < nIn; ++i) input.push_back(b64decode(p[idx++]));
            if (p[idx] != "->") { fail("malformed SHRINK row (missing ->)", line); continue; }
            ++idx;  // skip "->"
            int nOut = std::stoi(p[idx++]);
            std::vector<std::string> expected;
            expected.reserve(static_cast<size_t>(nOut));
            for (int i = 0; i < nOut; ++i) expected.push_back(b64decode(p[idx++]));

            std::vector<std::string> got = srp::shrink(input);
            if (static_cast<int>(got.size()) != nOut) {
                fail("shrink rowcount got=" + std::to_string(got.size()) + " exp=" + std::to_string(nOut), line);
            } else {
                for (size_t i = 0; i < got.size(); ++i) {
                    if (got[i] != expected[i]) {
                        fail("shrink row[" + std::to_string(i) + "] got='" + got[i] + "' exp='" + expected[i] + "'", line);
                        break;
                    }
                }
            }

        } else if (tag == "SYM") {
            // SYM <width> <height> <symmetrical(0|1)> <cellsB64>
            int w = std::stoi(p[1]);
            int h = std::stoi(p[2]);
            bool exp = (std::stoi(p[3]) != 0);
            std::string cells = b64decode(p[4]);
            // Element equality by value: cell a equals cell b iff the chars match.
            auto equalsAt = [&](int a, int b) -> bool {
                return cells[static_cast<size_t>(a)] == cells[static_cast<size_t>(b)];
            };
            bool got = srp::isSymmetrical(w, h, equalsAt);
            if (got != exp)
                fail(std::string("isSymmetrical got=") + (got ? "1" : "0") + " exp=" + (exp ? "1" : "0"), line);

        } else {
            fail("unknown TAG " + tag, line);
        }
    }

    std::cout << "ShapedRecipePattern checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
