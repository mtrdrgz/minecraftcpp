// Parity test for net.minecraft.util.LinearCongruentialGenerator.
// Ground truth: tools/LcgParity.java vs the real class. Verifies the two
// embedded constants (MULTIPLIER/INCREMENT) and next() over a thorough battery,
// including the chained BiomeManager.getFiddledDistance call shape. All values are
// signed 64-bit longs compared bit-for-bit.
//
//   lcg_parity --cases mcpp/build/lcg.tsv

#include "LinearCongruentialGenerator.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using mc::util::LinearCongruentialGenerator;

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Java prints longs in decimal; the full 64-bit range includes Long.MIN_VALUE
// which std::stoll handles. Parse via unsigned-aware path to be safe across the
// signed range.
int64_t pl(const std::string& s) {
    return static_cast<int64_t>(std::stoll(s));
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) {
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: lcg_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eq = [&](int64_t got, int64_t exp, const std::string& l) {
        if (std::bit_cast<uint64_t>(got) != std::bit_cast<uint64_t>(exp)) {
            fail(l + "  got=" + std::to_string(got));
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "MULTIPLIER") {
            eq(LinearCongruentialGenerator::MULTIPLIER, pl(p[1]), line);
        } else if (t == "INCREMENT") {
            eq(LinearCongruentialGenerator::INCREMENT, pl(p[1]), line);
        } else if (t == "NEXT") {
            // NEXT  seed  c  result
            eq(LinearCongruentialGenerator::next(pl(p[1]), pl(p[2])), pl(p[3]), line);
        } else if (t == "CHAIN") {
            // CHAIN  seed  x  y  z  v6  v7  v8
            const int64_t s = pl(p[1]);
            const int64_t x = pl(p[2]);
            const int64_t y = pl(p[3]);
            const int64_t z = pl(p[4]);
            int64_t v = s;
            v = LinearCongruentialGenerator::next(v, x);
            v = LinearCongruentialGenerator::next(v, y);
            v = LinearCongruentialGenerator::next(v, z);
            v = LinearCongruentialGenerator::next(v, x);
            v = LinearCongruentialGenerator::next(v, y);
            v = LinearCongruentialGenerator::next(v, z);
            eq(v, pl(p[5]), line);
            v = LinearCongruentialGenerator::next(v, s);
            eq(v, pl(p[6]), line);
            v = LinearCongruentialGenerator::next(v, s);
            eq(v, pl(p[7]), line);
        } else {
            fail(line + "  (unknown tag)");
        }
    }

    std::cout << "LinearCongruentialGenerator cases=" << total
              << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
