// Bit-exact parity for the PURE char/string validators of
// net.minecraft.resources.Identifier (formerly ResourceLocation) against the
// REAL net.minecraft methods (tools/ResourceLocationCharsParity.java ground truth).
//
// Predicates verified (ported in resources/IdentifierChars.h):
//   validPathChar(char) / validNamespaceChar(char) / isAllowedInIdentifier(char)
//   isValidPath(String) / isValidNamespace(String)
//
// TSV rows (tab-separated):
//   CHAR  <codeunit-dec>  <validPathChar 0/1>  <validNamespaceChar 0/1>  <isAllowedInIdentifier 0/1>
//   PATH  <input-as-UTF8-HEX>  <isValidPath 0/1>
//   NS    <input-as-UTF8-HEX>  <isValidNamespace 0/1>
//
//   rl_chars_parity [--cases mcpp/build/rl_chars.tsv]
#include "IdentifierChars.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::resources;

namespace {
std::string unhex(const std::string& s) {
    std::string out;
    out.reserve(s.size() / 2);
    for (size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back(static_cast<char>(std::stoi(s.substr(i, 2), nullptr, 16)));
    return out;
}
// Split a line into fields by '\t'. Preserves empty fields (e.g. empty hex for "").
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
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/rl_chars.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> fields = split(line);
        const std::string& tag = fields[0];

        if (tag == "CHAR") {
            // CHAR \t codeunit \t vp \t vn \t ai
            if (fields.size() != 5) { std::cerr << "bad CHAR row: " << line << "\n"; ++mismatches; continue; }
            ++cases;
            int cu = std::stoi(fields[1]);
            int wantVp = std::stoi(fields[2]);
            int wantVn = std::stoi(fields[3]);
            int wantAi = std::stoi(fields[4]);

            int gotVp = validPathChar(cu) ? 1 : 0;
            int gotVn = validNamespaceChar(cu) ? 1 : 0;
            int gotAi = isAllowedInIdentifier(cu) ? 1 : 0;

            if (gotVp != wantVp || gotVn != wantVn || gotAi != wantAi) {
                ++mismatches;
                std::cerr << "CHAR-MISMATCH cu=" << cu
                          << " validPathChar got " << gotVp << " want " << wantVp
                          << " | validNamespaceChar got " << gotVn << " want " << wantVn
                          << " | isAllowedInIdentifier got " << gotAi << " want " << wantAi
                          << "\n";
            }
        } else if (tag == "PATH") {
            // PATH \t hex \t isValidPath
            if (fields.size() != 3) { std::cerr << "bad PATH row: " << line << "\n"; ++mismatches; continue; }
            ++cases;
            std::string input = unhex(fields[1]);
            int want = std::stoi(fields[2]);
            int got = isValidPath(input) ? 1 : 0;
            if (got != want) {
                ++mismatches;
                std::cerr << "PATH-MISMATCH \"" << input << "\" got " << got << " want " << want << "\n";
            }
        } else if (tag == "NS") {
            // NS \t hex \t isValidNamespace
            if (fields.size() != 3) { std::cerr << "bad NS row: " << line << "\n"; ++mismatches; continue; }
            ++cases;
            std::string input = unhex(fields[1]);
            int want = std::stoi(fields[2]);
            int got = isValidNamespace(input) ? 1 : 0;
            if (got != want) {
                ++mismatches;
                std::cerr << "NS-MISMATCH \"" << input << "\" got " << got << " want " << want << "\n";
            }
        } else {
            // Unknown tag — ignore (forward compatible).
            continue;
        }
    }

    std::cout << "ResourceLocationChars cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
