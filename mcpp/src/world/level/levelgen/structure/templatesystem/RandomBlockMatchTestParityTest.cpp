// Parity test for the C++ RandomBlockMatchTest port
//   (world/level/levelgen/structure/templatesystem/RandomBlockMatchTest.h).
//
// Ground truth: mcpp/tools/RandomBlockMatchTestParity.java, which drives the REAL
// decompiled net.minecraft.world.level.levelgen.structure.templatesystem.
// RandomBlockMatchTest from client.jar.
//
// Each row replays one test() call. The C++ port reconstructs the same
// LegacyRandomSource(seed), runs the ported predicate, then draws one nextLong()
// exactly as the Java driver did. Two things are compared bit-exactly:
//   * the boolean result of test()
//   * the post-call nextLong() (the witness that the && short-circuit consumed
//     the right number of RNG draws — zero floats on no-match, one on match)
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TAG layout (tab-separated):
//   RBMT  <seed>  <stateBlockIdx>  <testBlockIdx>  <probBits>  <result0|1>  <afterLong>

#include "world/level/levelgen/structure/templatesystem/RandomBlockMatchTest.h"

#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::structure::templatesystem::RandomBlockMatchTest;

namespace {

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;  // blank

    if (tag == "RBMT") {
        int64_t seed = 0;
        int stateIdx = 0, testIdx = 0;
        uint32_t probBits = 0;
        int expectedResult = 0;
        int64_t expectedAfter = 0;
        in >> seed >> stateIdx >> testIdx >> probBits >> expectedResult >> expectedAfter;

        const float probability = std::bit_cast<float>(probBits);
        RandomBlockMatchTest rule(testIdx, probability);

        LegacyRandomSource random(seed);
        const bool result = rule.test(stateIdx, random);
        const int64_t after = random.nextLong();

        if ((result ? 1 : 0) != expectedResult) {
            err = "RBMT result expected=" + std::to_string(expectedResult) +
                  " got=" + std::to_string(result ? 1 : 0);
            return false;
        }
        if (after != expectedAfter) {
            err = "RBMT afterLong expected=" + std::to_string(expectedAfter) +
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
        std::cerr << "RandomBlockMatchTest: cannot open " << path << "\n";
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
    std::cout << "RandomBlockMatchTest checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks (no Mojang files): exercise the && short-circuit and the strict <.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // No-match block: test() is false regardless of probability and consumes NO
    // RNG draw. The next nextLong() must equal a fresh source's first nextLong().
    {
        RandomBlockMatchTest rule(/*blockId*/ 3, /*p*/ 2.0f);  // p>1 forces draw on match
        LegacyRandomSource a(42);
        bool r = rule.test(/*stateBlockId*/ 7, a);  // 7 != 3 -> no match
        int64_t after = a.nextLong();
        LegacyRandomSource b(42);
        int64_t fresh = b.nextLong();
        check(!r, "no-match result false");
        check(after == fresh, "no-match consumes 0 draws");
    }

    // Match with p>1: nextFloat() (in [0,1)) is always < p, so result true and
    // exactly ONE float is consumed before the nextLong().
    {
        RandomBlockMatchTest rule(/*blockId*/ 5, /*p*/ 2.0f);
        LegacyRandomSource a(42);
        bool r = rule.test(/*stateBlockId*/ 5, a);  // match
        int64_t after = a.nextLong();
        LegacyRandomSource b(42);
        (void)b.nextFloat();  // one float consumed by the match
        int64_t expect = b.nextLong();
        check(r, "match p>1 result true");
        check(after == expect, "match consumes exactly 1 float");
    }

    // Match with p==0: nextFloat() in [0,1) is never < 0, so result false, but a
    // float IS still consumed (block matched, so the RHS was evaluated).
    {
        RandomBlockMatchTest rule(/*blockId*/ 1, /*p*/ 0.0f);
        LegacyRandomSource a(1);
        bool r = rule.test(/*stateBlockId*/ 1, a);  // match, but p==0
        int64_t after = a.nextLong();
        LegacyRandomSource b(1);
        (void)b.nextFloat();
        int64_t expect = b.nextLong();
        check(!r, "match p==0 result false");
        check(after == expect, "match p==0 still consumes 1 float");
    }

    std::cout << "RandomBlockMatchTest self-checks mismatches=" << mism << "\n";
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
