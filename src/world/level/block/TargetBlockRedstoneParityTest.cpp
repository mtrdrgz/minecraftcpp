// Parity test for the pure redstone-strength helper of
//   net.minecraft.world.level.block.TargetBlock (MC 26.1.2).
// Ground truth: tools/TargetBlockRedstoneParity.java drives the REAL
// TargetBlock.getRedstoneStrength(BlockHitResult, Vec3) (private, reflective) -> TSV.
// This test recomputes with the C++ port (world/level/block/TargetBlockRedstone.h)
// and compares the integer redstone strength exactly. Hit-location doubles are
// exchanged as raw IEEE-754 bits via std::bit_cast so the input is bit-identical.
//
//   target_block_redstone_parity --cases mcpp/build/target_block_redstone.tsv

#include "TargetBlockRedstone.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace tb = mc::block_target;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i32(const std::string& s) { return static_cast<int>(std::stoll(s)); }
uint64_t hx64(const std::string& s) { return static_cast<uint64_t>(std::stoull(s, nullptr, 16)); }
double bd(const std::string& s) { return std::bit_cast<double>(hx64(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: target_block_redstone_parity --cases <tsv>\n";
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

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "STR") {
            // dirOrd xbits ybits zbits expectedStrength
            tb::Direction d = static_cast<tb::Direction>(i32(p[1]));
            double x = bd(p[2]);
            double y = bd(p[3]);
            double z = bd(p[4]);
            int got = tb::getRedstoneStrength(d, x, y, z);
            int exp = i32(p[5]);
            if (got != exp) fail(line + " got=" + std::to_string(got));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "TargetBlockRedstone checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
