// Parity test for the pure slot-picking geometry of
//   net.minecraft.world.level.block.SelectableSlotContainer (MC 26.1.2).
// Ground truth: tools/SelectableSlotContainerParity.java drives the REAL
// interface default + private-static methods (getHitSlot -> transitively
// getRelativeHitCoordinatesForBlockFace + getSection) -> TSV.
// This test recomputes with the C++ port
// (world/level/block/SelectableSlotContainer.h) and compares the
// present-flag + slot index exactly. Hit-location doubles are exchanged as raw
// IEEE-754 bits via std::bit_cast so the input is bit-identical.
//
//   selectable_slot_container_parity --cases mcpp/build/selectable_slot_container.tsv

#include "SelectableSlotContainer.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ss = mc::block_selectableslot;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream sstr(line);
    while (std::getline(sstr, it, '\t')) out.push_back(it);
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
        std::cerr << "usage: selectable_slot_container_parity --cases <tsv>\n";
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

        if (t == "SLOT") {
            // hitDirOrd bx by bz locXbits locYbits locZbits facingOrd rows columns present slot
            ss::Direction hitDir = static_cast<ss::Direction>(i32(p[1]));
            int bx = i32(p[2]);
            int by = i32(p[3]);
            int bz = i32(p[4]);
            double locX = bd(p[5]);
            double locY = bd(p[6]);
            double locZ = bd(p[7]);
            ss::Direction facing = static_cast<ss::Direction>(i32(p[8]));
            int rows = i32(p[9]);
            int columns = i32(p[10]);
            int expPresent = i32(p[11]);
            int expSlot = i32(p[12]);

            ss::HitSlot got = ss::getHitSlot(hitDir, bx, by, bz, locX, locY, locZ, facing, rows, columns);
            int gotPresent = got.present ? 1 : 0;
            int gotSlot = got.present ? got.slot : 0;
            if (gotPresent != expPresent || gotSlot != expSlot)
                fail(line + " gotPresent=" + std::to_string(gotPresent) + " gotSlot=" + std::to_string(gotSlot));
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SelectableSlotContainer checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
