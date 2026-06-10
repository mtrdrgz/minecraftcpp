// Parity test for net.minecraft.stats.StatFormatter (Minecraft 26.1.2).
//
// Ground truth: tools/StatFormatterParity.java vs the real StatFormatter constants.
// Each row carries (TAG, intValue, expectedString); we recompute the same string
// via stats/StatFormatter.h and compare it byte-for-byte.
//
//   stat_formatter_parity --cases mcpp/build/stat_formatter.tsv
//
// Output strings are emitted RAW by the GT tool (they never contain a tab), so the
// expected field is simply the remainder of the line after the second tab.

#include "StatFormatter.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace sf = mc::stats;

namespace {

// Split into exactly 3 fields: TAG, value, and the rest (the expected string, which
// is taken verbatim — it may itself be empty or contain no further tabs).
bool split3(const std::string& line, std::string& tag, std::string& val,
            std::string& expected) {
    size_t t1 = line.find('\t');
    if (t1 == std::string::npos) return false;
    size_t t2 = line.find('\t', t1 + 1);
    if (t2 == std::string::npos) return false;
    tag = line.substr(0, t1);
    val = line.substr(t1 + 1, t2 - (t1 + 1));
    expected = line.substr(t2 + 1);
    return true;
}

int32_t i32(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: stat_formatter_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        std::string tag, val, expected;
        if (!split3(line, tag, val, expected)) {
            std::cerr << "bad row: " << line << "\n";
            ++mism;
            ++n;
            continue;
        }
        ++n;
        int32_t v = i32(val);
        std::string got;
        if (tag == "DEFAULT") {
            got = sf::formatDefault(v);
        } else if (tag == "DIV10") {
            got = sf::formatDivideByTen(v);
        } else if (tag == "DISTANCE") {
            got = sf::formatDistance(v);
        } else if (tag == "TIME") {
            got = sf::formatTime(v);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }
        if (got != expected) {
            ++mism;
            if (mism <= 30)
                std::cerr << "MISMATCH [" << tag << "] value=" << val
                          << " expected=[" << expected << "] got=[" << got << "]\n";
        }
    }

    std::cout << "StatFormatter cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
