// Parity test for the C++ ProcessorRule port
//   (world/level/levelgen/structure/templatesystem/ProcessorRule.h).
//
// Ground truth: mcpp/tools/ProcessorRuleParity.java, which drives the REAL
// decompiled net.minecraft.world.level.levelgen.structure.templatesystem.
// ProcessorRule from client.jar, built from real RandomBlockStateMatchTest
// (input/location) and PosAlwaysTrueTest (position) sub-predicates.
//
// Each row replays one test() call. The C++ port reconstructs the same
// LegacyRandomSource(seed), runs the ported composite, then draws one nextLong()
// exactly as the Java driver did. Two things are compared bit-exactly:
//   * the boolean result of test()
//   * the post-call nextLong() (the witness that the chained && short-circuits
//     consumed the right number of RNG draws: arm1 draws iff its state matches;
//     arm2 draws iff arm1 passed AND arm2's state matches; arm3 never draws)
//
//   default        -> small self-checks (no Mojang files)
//   --cases <tsv>  -> verify every line of the generated reference
//
// TAG layout (tab-separated):
//   PR  <seed>  <inIdx>  <locIdx>  <inTgtIdx>  <inProbBits>  <locTgtIdx>  <locProbBits>  <result0|1>  <afterLong>

#include "world/level/levelgen/structure/templatesystem/ProcessorRule.h"

#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mc::levelgen::LegacyRandomSource;
using mc::levelgen::structure::templatesystem::PosAlwaysTrueTest;
using mc::levelgen::structure::templatesystem::ProcessorRule;
using mc::levelgen::structure::templatesystem::RandomBlockStateMatchTest;

namespace {

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;  // blank

    if (tag == "PR") {
        int64_t seed = 0;
        int inIdx = 0, locIdx = 0, inTgt = 0, locTgt = 0;
        uint32_t inProbBits = 0, locProbBits = 0;
        int expectedResult = 0;
        int64_t expectedAfter = 0;
        in >> seed >> inIdx >> locIdx >> inTgt >> inProbBits >> locTgt
           >> locProbBits >> expectedResult >> expectedAfter;

        const float inProb = std::bit_cast<float>(inProbBits);
        const float locProb = std::bit_cast<float>(locProbBits);

        RandomBlockStateMatchTest inputPredicate{inTgt, inProb};
        RandomBlockStateMatchTest locPredicate{locTgt, locProb};
        PosAlwaysTrueTest posPredicate{};
        ProcessorRule rule(inputPredicate, locPredicate, posPredicate);

        LegacyRandomSource random(seed);
        // Position arguments are ignored by PosAlwaysTrueTest; pass any ints.
        const bool result = rule.test(inIdx, locIdx, /*inTemplatePos*/ 1,
                                      /*worldPos*/ 4, /*reference*/ 7, random);
        const int64_t after = random.nextLong();

        if ((result ? 1 : 0) != expectedResult) {
            err = "PR result expected=" + std::to_string(expectedResult) +
                  " got=" + std::to_string(result ? 1 : 0);
            return false;
        }
        if (after != expectedAfter) {
            err = "PR afterLong expected=" + std::to_string(expectedAfter) +
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
        std::cerr << "ProcessorRule: cannot open " << path << "\n";
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
    std::cout << "ProcessorRule checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks (no Mojang files): exercise the three-armed && short-circuit and
// the RNG draw accounting.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    PosAlwaysTrueTest pos{};

    // arm1 no-match: input state id != input target id. test() is false and the
    // whole chain consumes ZERO RNG (arm1 short-circuits before its nextFloat).
    {
        ProcessorRule rule(RandomBlockStateMatchTest{/*tgt*/ 3, /*p*/ 2.0f},
                           RandomBlockStateMatchTest{/*tgt*/ 5, /*p*/ 2.0f}, pos);
        LegacyRandomSource a(42);
        bool r = rule.test(/*inState*/ 7, /*locState*/ 5, 0, 0, 0, a);  // 7 != 3
        int64_t after = a.nextLong();
        LegacyRandomSource b(42);
        int64_t fresh = b.nextLong();
        check(!r, "arm1 no-match result false");
        check(after == fresh, "arm1 no-match consumes 0 draws");
    }

    // arm1 match (p>1) but its draw < p so it passes; arm2 no-match: arm1 draws
    // ONE float, arm2 short-circuits before drawing, arm3 never draws.
    {
        ProcessorRule rule(RandomBlockStateMatchTest{/*tgt*/ 5, /*p*/ 2.0f},
                           RandomBlockStateMatchTest{/*tgt*/ 9, /*p*/ 2.0f}, pos);
        LegacyRandomSource a(42);
        bool r = rule.test(/*inState*/ 5, /*locState*/ 1, 0, 0, 0, a);  // arm1 match, arm2 1!=9
        int64_t after = a.nextLong();
        LegacyRandomSource b(42);
        (void)b.nextFloat();  // only arm1's float
        int64_t expect = b.nextLong();
        check(!r, "arm2 no-match result false");
        check(after == expect, "arm1 pass + arm2 no-match consumes exactly 1 float");
    }

    // Both arms match with p>1, posPredicate always true: exactly TWO floats
    // (arm1, arm2) consumed; PosAlwaysTrueTest draws nothing; result true.
    {
        ProcessorRule rule(RandomBlockStateMatchTest{/*tgt*/ 2, /*p*/ 2.0f},
                           RandomBlockStateMatchTest{/*tgt*/ 4, /*p*/ 2.0f}, pos);
        LegacyRandomSource a(7);
        bool r = rule.test(/*inState*/ 2, /*locState*/ 4, 0, 0, 0, a);  // both match
        int64_t after = a.nextLong();
        LegacyRandomSource b(7);
        (void)b.nextFloat();
        (void)b.nextFloat();
        int64_t expect = b.nextLong();
        check(r, "both arms match result true");
        check(after == expect, "both arms match consumes exactly 2 floats");
    }

    // arm1 match but p==0: arm1's nextFloat() in [0,1) is never < 0, so arm1
    // fails. arm1 still consumed ONE float; arm2 short-circuits. result false.
    {
        ProcessorRule rule(RandomBlockStateMatchTest{/*tgt*/ 1, /*p*/ 0.0f},
                           RandomBlockStateMatchTest{/*tgt*/ 1, /*p*/ 2.0f}, pos);
        LegacyRandomSource a(1);
        bool r = rule.test(/*inState*/ 1, /*locState*/ 1, 0, 0, 0, a);  // arm1 match but p==0
        int64_t after = a.nextLong();
        LegacyRandomSource b(1);
        (void)b.nextFloat();
        int64_t expect = b.nextLong();
        check(!r, "arm1 p==0 result false");
        check(after == expect, "arm1 p==0 still consumes 1 float, arm2 skipped");
    }

    std::cout << "ProcessorRule self-checks mismatches=" << mism << "\n";
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
