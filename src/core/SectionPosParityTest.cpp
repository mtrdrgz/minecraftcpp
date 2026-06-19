// Parity test for net.minecraft.core.SectionPos. Ground truth:
// tools/SectionPosParity.java vs the REAL 26.1.2 class.
//
// Verifies the already-certified codec surface (asLong / x/y/z /
// blockToSectionCoord / sectionToBlockCoord, reused read-only from core/PosCodec.h)
// AND the relative/offset surface newly ported in core/SectionPosOffset.h. All
// values are ints/longs compared bit-for-bit (via std::bit_cast where widths differ).
//
//   section_pos_parity --cases mcpp/build/section_pos.tsv

#include "core/PosCodec.h"
#include "core/SectionPosOffset.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace pc = mc::poscodec;
namespace sp = mc::sectionpos;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int       i(const std::string& s)  { return static_cast<int>(std::stoll(s)); }
long long ll(const std::string& s) { return std::stoll(s); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: section_pos_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI  = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != ll(exp)) fail(l + " got=" + std::to_string(got));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "SP_ASLONG") {
            eqI(pc::sectionPosAsLong(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        } else if (t == "SP_GET") {
            long long n = ll(p[1]);
            if (pc::sectionPosX(n) != ll(p[2]) || pc::sectionPosY(n) != ll(p[3]) || pc::sectionPosZ(n) != ll(p[4]))
                fail(line);
        } else if (t == "B2S") {
            eqI(pc::blockToSectionCoord(i(p[1])), p[2], line);
        } else if (t == "S2B") {
            eqI(pc::sectionToBlockCoord(i(p[1])), p[2], line);
        } else if (t == "S2B_OFF") {
            eqI(pc::sectionToBlockCoord(i(p[1]), i(p[2])), p[3], line);
        } else if (t == "SP_OFFSET") {
            eqI(sp::offset(ll(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        } else if (t == "SP_OFFSET_DIR") {
            auto dir = static_cast<mc::Direction>(i(p[2]));
            eqI(sp::offset(ll(p[1]), dir), p[3], line);
        } else if (t == "SP_INST_OFFSET") {
            sp::SectionCoord base{i(p[1]), i(p[2]), i(p[3])};
            sp::SectionCoord got = sp::offsetCoord(base, i(p[4]), i(p[5]), i(p[6]));
            if (got.x != ll(p[7]) || got.y != ll(p[8]) || got.z != ll(p[9])) fail(line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SectionPos cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
