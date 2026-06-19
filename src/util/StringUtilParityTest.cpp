// Parity test for the PURE methods of net.minecraft.util.StringUtil.
// Ground truth: tools/StringUtilParity.java vs the real class. Every method is
// recomputed via util/StringUtil.h and compared exactly.
//
//   stringutil_parity --cases mcpp/build/stringutil.tsv
//
// String fields in the TSV are encoded as concatenated %04x per UTF-16 code unit;
// sentinels "NULL" => null reference (std::nullopt), "EMPTY" => empty string.

#include "StringUtil.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <set>
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

// Decode the %04x-per-code-unit encoding (with NULL / EMPTY sentinels) into an optional
// UTF-16 string. NULL -> nullopt, EMPTY -> "".
std::optional<std::u16string> decode(const std::string& field) {
    if (field == "NULL") return std::nullopt;
    if (field == "EMPTY") return std::u16string();
    std::u16string out;
    for (size_t p = 0; p + 4 <= field.size(); p += 4) {
        std::string g = field.substr(p, 4);
        unsigned long v = std::stoul(g, nullptr, 16);
        out.push_back(static_cast<char16_t>(v));
    }
    return out;
}

// Re-encode a u16string the same way the GT tool does, so we can compare string outputs
// (TRUNC / TRIMCHAT) byte-for-byte against the expected encoded field.
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
    if (casesPath.empty()) { std::cerr << "usage: stringutil_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;

    // Collect the ground-truth whitespace set (WSCP rows) and our table's set, then
    // verify they are identical: catches any drift between the embedded .inc and the JDK.
    std::set<int32_t> gtWhitespace;

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF tsv
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "WSCP") {  // cp  (a code point the real isWhitespace marks true)
            int32_t cp = i(p[1]);
            gtWhitespace.insert(cp);
            // Our table must classify this cp as whitespace.
            bad = !su::isWhitespace(cp);
        } else if (tag == "ISWS") {  // cp bool
            bad = su::isWhitespace(i(p[1])) != b(p[2]);
        } else if (tag == "ISBLANK") {  // strHex bool
            bad = su::isBlank(decode(p[1])) != b(p[2]);
        } else if (tag == "ISNULLOREMPTY") {  // strHex bool
            bad = su::isNullOrEmpty(decode(p[1])) != b(p[2]);
        } else if (tag == "TRUNC") {  // strHex maxLen addDots outHex
            auto s = decode(p[1]);
            // truncate's input is never null in the GT tool.
            std::u16string out = su::truncateStringIfNecessary(s.value(), i(p[2]), b(p[3]));
            bad = encode(out) != p[4];
        } else if (tag == "LINECOUNT") {  // strHex int
            auto s = decode(p[1]);
            bad = su::lineCount(s.value()) != i(p[2]);
        } else if (tag == "TRIMCHAT") {  // strHex outHex
            auto s = decode(p[1]);
            bad = encode(su::trimChatMessage(s.value())) != p[2];
        } else if (tag == "ALLOWEDCHAR") {  // ch bool
            bad = su::isAllowedChatCharacter(i(p[1])) != b(p[2]);
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

    // Reverse check: every code unit our table marks whitespace must appear as a WSCP
    // row in the ground truth (no extra/invented entries in the .inc).
    long long extra = 0;
    for (char16_t w : su::STRINGUTIL_WHITESPACE_CODE_UNITS) {
        if (gtWhitespace.find(static_cast<int32_t>(w)) == gtWhitespace.end()) {
            ++extra;
            if (extra <= 20)
                std::cerr << "EXTRA whitespace in table not in GT: " << static_cast<int32_t>(w) << "\n";
        }
    }
    if (!gtWhitespace.empty()) {  // only enforce when WSCP rows were present
        mism += extra;
        n += extra;
    }

    std::cout << "StringUtil cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
