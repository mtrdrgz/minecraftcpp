// Parity test for the PURE static slot/redstone math of
// net.minecraft.world.inventory.AbstractContainerMenu — verifies the C++ port in
// world/inventory/AbstractContainerMenuMath.h reproduces the REAL statics
// bit-for-bit against tools/AbstractContainerMenuMathParity.java ground truth.
//
//   abstract_container_math_parity --cases mcpp/build/abstract_container_math.tsv
//
// TAGs (see AbstractContainerMenuMathParity.java):
//   QTYPE   <mask>                              <result>
//   QHEAD   <mask>                              <result>
//   QMASK   <header> <type>                     <result>
//   QPLACE  <size> <type> <count> <maxStack>    <result>
//   REDST   <containerSize> <nSlots> [slot:count:effMax]...  <result>
//
// All results are decimal int32 (the methods return int). The float math inside
// getQuickCraftPlaceCount (case 0) and getRedstoneSignalFromContainer is performed in
// float precision by the port exactly as Java does, so the returned ints match exactly.

#include "AbstractContainerMenuMath.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace inv = mc::world::inventory;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: abstract_container_math_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << msg << "\n";
    };
    auto check = [&](int got, int exp, const std::string& line) {
        if (got != exp)
            fail(line + " got=" + std::to_string(got) + " exp=" + std::to_string(exp));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "QTYPE") {
            // QTYPE <mask> <result>
            check(inv::getQuickcraftType(toI(p[1])), toI(p[2]), line);
        } else if (t == "QHEAD") {
            // QHEAD <mask> <result>
            check(inv::getQuickcraftHeader(toI(p[1])), toI(p[2]), line);
        } else if (t == "QMASK") {
            // QMASK <header> <type> <result>
            check(inv::getQuickcraftMask(toI(p[1]), toI(p[2])), toI(p[3]), line);
        } else if (t == "QPLACE") {
            // QPLACE <size> <type> <count> <maxStack> <result>
            int r = inv::getQuickCraftPlaceCount(toI(p[1]), toI(p[2]), toI(p[3]), toI(p[4]));
            check(r, toI(p[5]), line);
        } else if (t == "REDST") {
            // REDST <containerSize> <nSlots> [slot:count:effMax]...x nSlots <result>
            int32_t containerSize = toI(p[1]);
            int32_t nSlots = toI(p[2]);
            std::vector<inv::RedstoneSlot> slots(static_cast<size_t>(containerSize),
                                                 inv::RedstoneSlot{0, 0});
            // tokens p[3 .. 3+nSlots-1] are slot:count:effMax ; last field is result.
            for (int32_t i = 0; i < nSlots; ++i) {
                const std::string& tok = p[3 + i];
                // parse "slot:count:effMax"
                size_t c1 = tok.find(':');
                size_t c2 = tok.find(':', c1 + 1);
                int32_t slotIdx = toI(tok.substr(0, c1));
                int32_t count = toI(tok.substr(c1 + 1, c2 - c1 - 1));
                int32_t effMax = toI(tok.substr(c2 + 1));
                if (slotIdx >= 0 && slotIdx < containerSize) {
                    slots[static_cast<size_t>(slotIdx)] = inv::RedstoneSlot{count, effMax};
                }
            }
            int32_t exp = toI(p[3 + nSlots]);
            check(inv::getRedstoneSignalFromContainer(slots), exp, line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "AbstractContainerMenuMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
