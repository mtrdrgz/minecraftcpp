// Parity GATE for net.minecraft.world.level.block.RedStoneWireBlock's static
// power->color table: getColorForPower(int) and the private COLORS[16] table that
// backs it.
//
// Ground truth: tools/RedstoneWireColorParity.java drives the REAL RedStoneWireBlock
// (public accessor + reflected COLORS[]) and emits packed ARGB ints as signed decimal.
// The C++ side recomputes the identical float expression chain (power*power*0.7F-0.5F,
// Mth.clamp, ARGB.colorFromFloat) and compares 32-bit ints exactly.
//
// 1:1 traps exercised here:
//   * IEEE-754 single-precision float arithmetic (i/15.0F, power*power*0.7F, etc.)
//   * the ternary bias 0.4F vs 0.3F gated on (power > 0.0F)
//   * Mth.clamp(float)  == value<min?min:Math.min(value,max)  (NOT std::clamp order)
//   * float->int truncation via Mth.floor == (int)Math.floor(double)  in as8BitChannel
//   * ARGB byte packing with &0xFF masks
//
//   redstone_wire_color_parity --cases mcpp/build/redstone_wire_color.tsv

#include "world/level/block/RedStoneWireBlockColor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace blk = mc::world::level::block;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
// Java ints emitted as signed decimal; parse via long long then narrow so packed
// colors with the high (alpha) bit set round-trip exactly.
long long ll(const std::string& s) { return std::stoll(s); }
int       i(const std::string& s)  { return static_cast<int>(std::stoll(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: redstone_wire_color_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // Build the table once for the TABLE cross-check rows.
    const auto table = blk::buildRedstoneWireColors();

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, long long got) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << " got=" << got << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != ll(exp)) fail(l, got);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];

        if (t == "GETCOLOR") {                       // GETCOLOR <power> <argb>
            ++total;
            eqI(blk::getColorForPower(i(p[1])), p[2], line);
        } else if (t == "TABLE") {                   // TABLE <index> <argb>
            ++total;
            int idx = i(p[1]);
            long long got = (idx >= 0 && idx < static_cast<int>(table.size()))
                                ? static_cast<long long>(table[idx])
                                : -999999999LL;
            eqI(got, p[2], line);
        } else if (t == "TABLELEN") {                // TABLELEN <len>
            ++total;
            eqI(static_cast<long long>(table.size()), p[1], line);
        } else {
            ++total;
            fail("UNKNOWN_TAG " + t, 0);
        }
    }

    std::cout << "RedstoneWireColor checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
