// Parity test for net.minecraft.core.QuartPos — biome-quart coordinate packing.
// Ground truth: tools/QuartPosParity.java vs the real 26.1.2 class.
// All values are int and compared exactly.
//
//   quart_pos_parity --cases mcpp/build/quart_pos.tsv

#include "QuartPos.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace qp = mc::quartpos;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java emits ints in decimal; parse as int32_t (handles INT_MIN/INT_MAX).
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: quart_pos_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l, long long got) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << " got=" << got << "\n";
    };
    auto eq = [&](int32_t got, const std::string& exp, const std::string& l) {
        if (got != i(exp)) fail(l, got);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;
        const int32_t v = i(p[1]);

        if      (t == "FROM_BLOCK")   eq(qp::fromBlock(v),   p[2], line);
        else if (t == "QUART_LOCAL")  eq(qp::quartLocal(v),  p[2], line);
        else if (t == "TO_BLOCK")     eq(qp::toBlock(v),     p[2], line);
        else if (t == "FROM_SECTION") eq(qp::fromSection(v), p[2], line);
        else if (t == "TO_SECTION")   eq(qp::toSection(v),   p[2], line);
        else if (t == "RT_QB")        eq(qp::fromBlock(qp::toBlock(v)),     p[2], line);
        else if (t == "RT_QS")        eq(qp::toSection(qp::fromSection(v)), p[2], line);
        else { fail("UNKNOWN_TAG " + t, 0); }
    }

    std::cout << "QuartPos cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
