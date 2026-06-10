// VERIFY parity test for net.minecraft.core.QuartPos — biome-quart coordinate
// packing. This gate certifies the EXISTING engine header
// mcpp/src/core/QuartPos.h (mc::quartpos::*) bit-for-bit against ground truth
// produced by the REAL 26.1.2 class (tools/QuartPosVerifyParity.java).
//
//   quart_pos_verify_parity --cases mcpp/build/quart_pos_verify.tsv
//
// All QuartPos outputs are int. We parse Java's decimal output as int32_t (the
// already-truncated two's-complement result), recompute via the engine header,
// and compare bit-for-bit with std::bit_cast<uint32_t>. Java `>>` is an
// arithmetic (sign-propagating) shift; on every target this repo builds for
// (two's complement, llvm-mingw) C++ signed `>>` is likewise arithmetic and
// signed `<<`/`&` match Java int32 semantics, so the header's verbatim
// translation is well-defined here.

#include "core/QuartPos.h"

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
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java emits ints in decimal; parse via long long then narrow to int32_t so the
// full int range (INT_MIN..INT_MAX) round-trips exactly.
int32_t i32(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: quart_pos_verify_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, int32_t got, int32_t exp) {
        ++mism;
        if (shown++ < 40)
            std::cerr << "MISMATCH " << l << " got=" << got << " exp=" << exp << "\n";
    };
    // Bit-for-bit compare (ints): identical iff the raw bit patterns match.
    auto eq = [&](int32_t got, const std::string& expStr, const std::string& l) {
        const int32_t exp = i32(expStr);
        if (std::bit_cast<uint32_t>(got) != std::bit_cast<uint32_t>(exp)) fail(l, got, exp);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];

        if (t == "CONSTS") {
            // CONSTS \t BITS \t SIZE \t MASK \t SECTION_TO_QUARTS_BITS
            ++total;
            if (std::bit_cast<uint32_t>(qp::BITS) != std::bit_cast<uint32_t>(i32(p[1])) ||
                std::bit_cast<uint32_t>(qp::SIZE) != std::bit_cast<uint32_t>(i32(p[2])) ||
                std::bit_cast<uint32_t>(qp::MASK) != std::bit_cast<uint32_t>(i32(p[3])) ||
                std::bit_cast<uint32_t>(qp::SECTION_TO_QUARTS_BITS) !=
                    std::bit_cast<uint32_t>(i32(p[4]))) {
                ++mism;
                std::cerr << "MISMATCH " << line << " (constants)\n";
            }
            continue;
        }

        ++total;
        const int32_t v = i32(p[1]);

        if      (t == "FROM_BLOCK")   eq(qp::fromBlock(v),                       p[2], line);
        else if (t == "QUART_LOCAL")  eq(qp::quartLocal(v),                      p[2], line);
        else if (t == "TO_BLOCK")     eq(qp::toBlock(v),                         p[2], line);
        else if (t == "FROM_SECTION") eq(qp::fromSection(v),                     p[2], line);
        else if (t == "TO_SECTION")   eq(qp::toSection(v),                       p[2], line);
        else if (t == "RT_QB")        eq(qp::fromBlock(qp::toBlock(v)),          p[2], line);
        else if (t == "RT_QS")        eq(qp::toSection(qp::fromSection(v)),      p[2], line);
        else {
            ++mism;
            if (shown++ < 40) std::cerr << "UNKNOWN_TAG " << t << "\n";
        }
    }

    std::cout << "QuartPosVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
