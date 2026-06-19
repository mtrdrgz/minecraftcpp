// Parity test for the StringUtil helpers NOT covered by stringutil_parity:
//   stripColor, filterText (both overloads), isValidPlayerName.
// Also re-verifies truncateStringIfNecessary edge cases and lineCount against
// util/StringUtil.h.
//
// Ground truth: tools/StringUtilExtraParity.java vs the REAL net.minecraft.util.StringUtil.
//
//   hash_common_parity --cases mcpp/build/hash_common.tsv
//
// String fields in the TSV are encoded as concatenated %04x per UTF-16 code unit;
// sentinel "EMPTY" => the empty string "".

#include "util/StringUtil.h"
#include "util/StringUtilExtra.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace su = mc::util::stringutil;

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

bool b(const std::string& s) { return std::stoll(s) != 0; }

// Decode the %04x-per-code-unit encoding (with EMPTY sentinel) into a UTF-16 string.
std::u16string decode(const std::string& field) {
    if (field == "EMPTY") return std::u16string();
    std::u16string out;
    for (size_t p = 0; p + 4 <= field.size(); p += 4) {
        unsigned long v = std::stoul(field.substr(p, 4), nullptr, 16);
        out.push_back(static_cast<char16_t>(v));
    }
    return out;
}

// Re-encode a u16string the same way the GT tool does, so string outputs can be compared
// byte-for-byte against the expected encoded field.
std::string encode(const std::u16string& s) {
    if (s.empty()) return "EMPTY";
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() * 4);
    for (char16_t c : s) {
        uint16_t v = static_cast<uint16_t>(c);
        out.push_back(hexd[(v >> 12) & 0xF]);
        out.push_back(hexd[(v >> 8) & 0xF]);
        out.push_back(hexd[(v >> 4) & 0xF]);
        out.push_back(hexd[v & 0xF]);
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: hash_common_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF tsv
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "STRIPCOLOR") {  // inHex outHex
            std::u16string out = su::stripColor(decode(p[1]));
            bad = encode(out) != p[2];
        } else if (tag == "FILTER") {  // inHex multiline outHex
            std::u16string out = su::filterText(decode(p[1]), b(p[2]));
            bad = encode(out) != p[3];
        } else if (tag == "VALIDNAME") {  // inHex bool
            bad = su::isValidPlayerName(decode(p[1])) != b(p[2]);
        } else if (tag == "TRUNC") {  // inHex maxLen addDots outHex
            std::u16string out = su::truncateStringIfNecessary(decode(p[1]), i(p[2]), b(p[3]));
            bad = encode(out) != p[4];
        } else if (tag == "LINECOUNT") {  // inHex int
            bad = su::lineCount(decode(p[1])) != i(p[2]);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "StringUtilExtra cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
