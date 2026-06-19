// Parity test for the C++ JungleTempleStoneSelector port
//   (world/level/levelgen/structure/structures/JungleTempleStoneSelector.h).
//
// Ground truth: mcpp/tools/JungleTempleStoneSelectorParity.java, which drives the
// REAL decompiled inner class
//   net.minecraft.world.level.levelgen.structure.structures
//       .JungleTemplePiece$MossStoneSelector
// from client.jar (the per-cell randomiser for the jungle temple cobblestone
// maze, called by StructurePiece.generateBox(..., BlockSelector)).
//
// Each row replays one case: a seed, then `count` consecutive next() calls on a
// freshly seeded LegacyRandomSource, the block code selected by each, and a
// trailing nextLong(). The C++ port reconstructs the same LegacyRandomSource,
// runs the ported selector `count` times, then draws one nextLong() exactly as
// the Java driver did. Compared bit-exactly:
//   * each per-cell block code (0 cobblestone / 1 mossy cobblestone), i.e. the
//     `nextFloat() < 0.4f` branch selection
//   * the post-sequence nextLong() (witness that each next() consumed exactly
//     ONE nextFloat(), so the RNG advanced bit-identically over the whole maze)
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TAG layout (tab-separated):
//   JTSS  <seed>  <count>  <code0> <code1> ... <code{count-1}>  <afterLong>

#include "world/level/levelgen/structure/structures/JungleTempleStoneSelector.h"

#include "world/level/levelgen/RandomSource.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::structure::structures::JungleTempleBlock;
using mc::levelgen::structure::structures::JungleTempleStoneSelector;

namespace {

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;  // blank

    if (tag == "JTSS") {
        int64_t seed = 0;
        int count = 0;
        in >> seed >> count;
        if (count < 0 || count > 100000) {
            err = "JTSS bad count=" + std::to_string(count);
            return false;
        }
        std::vector<int> expectedCodes(static_cast<size_t>(count));
        for (int k = 0; k < count; ++k) in >> expectedCodes[static_cast<size_t>(k)];
        int64_t expectedAfter = 0;
        in >> expectedAfter;

        JungleTempleStoneSelector selector;
        LegacyRandomSource random(seed);
        // Vary the (ignored) coordinate/edge args across calls to exercise that
        // they have no effect, exactly as the Java driver does per case.
        for (int k = 0; k < count; ++k) {
            const bool isEdge = (k & 1) != 0;
            selector.next(random, k, k * 3, -k, isEdge);
            const int code = static_cast<int>(selector.getNext());
            const int want = expectedCodes[static_cast<size_t>(k)];
            if (code != want) {
                err = "JTSS code[" + std::to_string(k) + "] expected=" +
                      std::to_string(want) + " got=" + std::to_string(code);
                return false;
            }
        }
        const int64_t after = random.nextLong();
        if (after != expectedAfter) {
            err = "JTSS afterLong expected=" + std::to_string(expectedAfter) +
                  " got=" + std::to_string(after);
            return false;
        }
        return true;
    }

    err = "unknown tag: " + tag;
    return false;
}

int runCases(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "JungleTempleStoneSelector: cannot open " << path << "\n";
        return 2;
    }
    std::string line;
    int n = 0, mism = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        ++n;
        std::string err;
        if (!verifyLine(line, err)) {
            if (mism < 20) std::cerr << "MISMATCH: " << err << " | " << line << "\n";
            ++mism;
        }
    }
    std::cout << "JungleTempleStoneSelector checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks (no Mojang files): independently confirm the threshold and the
// one-float-per-call RNG consumption.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // Branch selection must match `nextFloat() < 0.4f` computed independently,
    // and exactly one float must be consumed per next() call.
    {
        JungleTempleStoneSelector selector;
        LegacyRandomSource a(12345);
        LegacyRandomSource b(12345);
        for (int k = 0; k < 64; ++k) {
            selector.next(a, k, 0, 0, false);
            const float f = b.nextFloat();
            const JungleTempleBlock want =
                f < 0.4F ? JungleTempleBlock::Cobblestone
                         : JungleTempleBlock::MossyCobblestone;
            check(selector.getNext() == want, "branch matches nextFloat()<0.4f");
        }
        // After 64 calls both sources must be at the same position.
        check(a.nextLong() == b.nextLong(), "one float consumed per next()");
    }

    // Coordinates / isEdge are ignored: same seed -> same sequence regardless.
    {
        JungleTempleStoneSelector s1;
        JungleTempleStoneSelector s2;
        LegacyRandomSource r1(777);
        LegacyRandomSource r2(777);
        bool same = true;
        for (int k = 0; k < 32; ++k) {
            s1.next(r1, 0, 0, 0, false);
            s2.next(r2, 999, -42, 17, true);
            if (s1.getNext() != s2.getNext()) same = false;
        }
        check(same, "world coords / isEdge are ignored");
    }

    std::cout << "JungleTempleStoneSelector self-checks mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath.empty()) return runCases(casesPath);
    return runSelfChecks();
}
