// Parity test for mc::levelgen::Xoroshiro128PlusPlus
// (world/level/levelgen/RandomSource.h).
//
// Ground truth: mcpp/tools/Xoroshiro128Parity.java, which runs the REAL
// decompiled net.minecraft.world.level.levelgen.Xoroshiro128PlusPlus from
// client.jar and emits, for each seed pair, the full nextLong() draw sequence.
// This test reconstructs the same Xoroshiro128PlusPlus(seedLo, seedHi) and pulls
// draws in order, comparing each returned long BIT-FOR-BIT (std::bit_cast).
//
// This certifies the rotateLeft (Long.rotateLeft) + XOR + left-shift state update
// and the zero-seed fallback (seedLo|seedHi == 0 -> GOLDEN/SILVER constants).
//
//   default        -> tiny self-checks (no Mojang files)
//   --cases <tsv>  -> verify every SEQ row of the generated reference
//
// TAG layout (tab-separated):
//   SEQ  <seedLoHex> <seedHiHex> <step>  <nextLongHex>
// where the hex fields are 16-digit raw 64-bit patterns.

#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

using mc::levelgen::Xoroshiro128PlusPlus;

namespace {

// Parse a (possibly 16-digit) hex string into its raw 64-bit pattern, then
// reinterpret as the signed long Java's constructor / nextLong() use.
int64_t parseLongHex(const std::string& s) {
    uint64_t bits = std::stoull(s, nullptr, 16);
    return std::bit_cast<int64_t>(bits);
}

std::string hex16(int64_t v) {
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%016llx",
                  static_cast<unsigned long long>(std::bit_cast<uint64_t>(v)));
    return std::string(buf);
}

int runCases(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        std::cerr << "Xoroshiro128: cannot open " << path << "\n";
        return 2;
    }

    std::string line;
    int n = 0, mism = 0;

    // The reference emits one seed pair's draws contiguously in ascending step
    // order. We (re)build the generator on step 0 and advance it once per row,
    // so the running C++ state must match every ground-truth draw in lockstep.
    bool haveRng = false;
    Xoroshiro128PlusPlus rng(0, 0);  // placeholder; replaced on first step==0
    int64_t curLo = 0, curHi = 0;
    int expectStep = 0;

    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::istringstream in(line);
        std::string tag;
        in >> tag;
        if (tag.empty()) continue;
        if (tag != "SEQ") {
            if (mism < 20) std::cerr << "MISMATCH: unknown tag: " << tag << "\n";
            ++mism;
            ++n;
            continue;
        }

        std::string loHex, hiHex, valHex;
        int step = 0;
        in >> loHex >> hiHex >> step >> valHex;
        int64_t seedLo = parseLongHex(loHex);
        int64_t seedHi = parseLongHex(hiHex);
        int64_t expected = parseLongHex(valHex);
        ++n;

        if (step == 0) {
            rng = Xoroshiro128PlusPlus(seedLo, seedHi);
            haveRng = true;
            curLo = seedLo;
            curHi = seedHi;
            expectStep = 0;
        } else if (!haveRng || seedLo != curLo || seedHi != curHi || step != expectStep) {
            // Out-of-order or mismatched seed grouping: rebuild from scratch by
            // replaying the chain so the test is robust to any row interleaving.
            rng = Xoroshiro128PlusPlus(seedLo, seedHi);
            for (int i = 0; i < step; ++i) rng.nextLong();
            curLo = seedLo;
            curHi = seedHi;
            haveRng = true;
            expectStep = step;
        }

        int64_t got = rng.nextLong();
        ++expectStep;

        if (std::bit_cast<uint64_t>(got) != std::bit_cast<uint64_t>(expected)) {
            if (mism < 20) {
                std::cerr << "MISMATCH: seedLo=" << loHex << " seedHi=" << hiHex
                          << " step=" << step << " expected=" << valHex
                          << " got=" << hex16(got) << "\n";
            }
            ++mism;
        }
    }

    std::cout << "Xoroshiro128 cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

// Self-checks without Mojang files: known canonical Xoroshiro128++ outputs and
// the zero-seed fallback, computed independently from the algorithm definition.
int runSelfChecks() {
    int mism = 0;
    auto check = [&](bool ok, const char* what) {
        if (!ok) {
            std::cerr << "SELFCHECK FAIL: " << what << "\n";
            ++mism;
        }
    };

    // Reference scalar Xoroshiro128++ (Long.rotateLeft semantics) to cross-check
    // the engine implementation on a couple of seed pairs.
    auto rotl = [](uint64_t v, int b) -> uint64_t {
        return (v << b) | (v >> (64 - b));
    };
    auto refNext = [&](uint64_t& s0r, uint64_t& s1r) -> uint64_t {
        uint64_t s0 = s0r, s1 = s1r;
        uint64_t result = rotl(s0 + s1, 17) + s0;
        s1 ^= s0;
        s0r = rotl(s0, 49) ^ s1 ^ (s1 << 21);
        s1r = rotl(s1, 28);
        return result;
    };

    // (1,2) seed: no fallback. Compare first 8 draws against the reference.
    {
        Xoroshiro128PlusPlus rng(1, 2);
        uint64_t r0 = 1, r1 = 2;
        for (int i = 0; i < 8; ++i) {
            int64_t got = rng.nextLong();
            int64_t exp = std::bit_cast<int64_t>(refNext(r0, r1));
            check(got == exp, "seed(1,2) draw matches reference");
        }
    }

    // (0,0) seed: must fall back to (GOLDEN, SILVER) before the first draw.
    {
        Xoroshiro128PlusPlus rng(0, 0);
        uint64_t r0 = std::bit_cast<uint64_t>(
            mc::levelgen::RandomSupport::GOLDEN_RATIO_64);
        uint64_t r1 = std::bit_cast<uint64_t>(
            mc::levelgen::RandomSupport::SILVER_RATIO_64);
        for (int i = 0; i < 8; ++i) {
            int64_t got = rng.nextLong();
            int64_t exp = std::bit_cast<int64_t>(refNext(r0, r1));
            check(got == exp, "seed(0,0) fallback draw matches reference");
        }
    }

    std::cout << "Xoroshiro128 self-checks mismatches=" << mism << "\n";
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
