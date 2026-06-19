// Parity test for the pure vertical-velocity math in
// net.minecraft.world.level.block.HoneyBlock (getOldDeltaY / getNewDeltaY).
// Ground truth: tools/HoneyBlockParity.java vs the real class (private statics invoked
// reflectively). Recomputes both with the C++ port and compares bit-for-bit (doubles
// as raw IEEE-754 bits via std::bit_cast).
//
//   honey_block_parity --cases mcpp/build/honey_block.tsv

#include "HoneyBlock.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// TSV doubles are raw IEEE-754 bits as a (possibly negative) decimal long.
double db(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoll(s))); }
uint64_t bits(double v) { return std::bit_cast<uint64_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: honey_block_parity --cases <tsv>\n";
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
    auto eqD = [&](double got, const std::string& expBits, const std::string& l) {
        const uint64_t exp = static_cast<uint64_t>(std::stoll(expBits));
        if (bits(got) != exp)
            fail(l + " gotbits=" + std::to_string(bits(got)) + " expbits=" + std::to_string(exp));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "OLD") {
            // <deltaY bits>  <getOldDeltaY bits>
            eqD(mc::block::honeyGetOldDeltaY(db(p[1])), p[2], line);
        } else if (t == "NEW") {
            // <deltaY bits>  <getNewDeltaY bits>
            eqD(mc::block::honeyGetNewDeltaY(db(p[1])), p[2], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "HoneyBlock checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
