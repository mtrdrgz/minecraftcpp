// Byte-exact parity gate for the C++ port of
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//        .SmoothStoneSelector
// against ground truth emitted by mcpp/tools/SmoothStoneSelectorParity.java
// (which drives the REAL decompiled class from client.jar).
//
//   --cases <tsv>
//   prints:  SmoothStoneSelector checks=N mismatches=M
//   exit code: nonzero iff M>0
//
// Row formats (tab-separated):
//   SSSINIT  <airKind>
//        the base StructurePiece.BlockSelector initial next == Blocks.AIR.
//   SSS  <seed>  <isEdgeMask>  <k0> k1 ... k(N-1)
//        N successive selector.nextBlock() calls driven by one continuously
//        advancing XoroshiroRandomSource(seed); isEdgeMask bit i is the isEdge
//        arg for call i (LSB = call 0). k_i is the StrongholdBlock kind of
//        getNext() after call i.
//
// The kind id codes match the Java driver's kindOf():
//   0 AIR, 1 CAVE_AIR, 2 CRACKED_STONE_BRICKS, 3 MOSSY_STONE_BRICKS,
//   4 INFESTED_STONE_BRICKS, 5 STONE_BRICKS.

#include "world/level/levelgen/structure/structures/SmoothStoneSelector.h"
#include "world/level/levelgen/RandomSource.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::XoroshiroRandomSource;
using mc::levelgen::structure::SmoothStoneSelector;
using mc::levelgen::structure::StrongholdBlock;

static int kindCode(StrongholdBlock b) {
    switch (b) {
        case StrongholdBlock::AIR:                   return 0;
        case StrongholdBlock::CAVE_AIR:              return 1;
        case StrongholdBlock::CRACKED_STONE_BRICKS:  return 2;
        case StrongholdBlock::MOSSY_STONE_BRICKS:    return 3;
        case StrongholdBlock::INFESTED_STONE_BRICKS: return 4;
        case StrongholdBlock::STONE_BRICKS:          return 5;
    }
    return -1;
}

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (!casesPath) {
        std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "cannot open cases file: %s\n", casesPath);
        return 2;
    }

    long long checks = 0;
    long long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::istringstream ss(line);
        std::string tag;
        ss >> tag;

        if (tag == "SSSINIT") {
            int expected = 0;
            ss >> expected;
            // The base BlockSelector initial value, before any nextBlock() call.
            SmoothStoneSelector sel;
            int got = kindCode(sel.getNext());
            ++checks;
            if (got != expected) {
                ++mismatches;
                std::fprintf(stderr, "SSSINIT mismatch: got=%d expected=%d\n", got, expected);
            }
        } else if (tag == "SSS") {
            long long seed = 0;
            unsigned long long isEdgeMask = 0;
            ss >> seed >> isEdgeMask;
            std::vector<int> expected;
            int k;
            while (ss >> k) expected.push_back(k);

            XoroshiroRandomSource rng(static_cast<int64_t>(seed));
            SmoothStoneSelector sel;
            for (std::size_t i = 0; i < expected.size(); ++i) {
                bool isEdge = ((isEdgeMask >> i) & 1ULL) != 0ULL;
                sel.nextBlock(rng, static_cast<int>(i), static_cast<int>(i) * 2,
                              static_cast<int>(i) * 3, isEdge);
                int got = kindCode(sel.getNext());
                ++checks;
                if (got != expected[i]) {
                    ++mismatches;
                    std::fprintf(stderr,
                        "SSS mismatch seed=%lld call=%zu isEdge=%d got=%d expected=%d\n",
                        seed, i, isEdge ? 1 : 0, got, expected[i]);
                }
            }
        }
        // unknown tags ignored
    }

    std::printf("SmoothStoneSelector checks=%lld mismatches=%lld\n", checks, mismatches);
    return mismatches == 0 ? 0 : 1;
}
