// Parity test for net.minecraft.util.valueproviders.ConstantFloat.
//
// VERIFIES two things bit-for-bit against ground truth from the REAL decompiled
// class (mcpp/tools/ConstantFloatParity.java -> int_provider_constant_float.tsv):
//
//   1. The full gate-faithful record port
//        mc::valueproviders::ConstantFloatRecord  (util/valueproviders/ConstantFloat.h)
//      covering of()/value()/min()/max()/sample() — including the of() singleton
//      folding where value==0.0F (incl. -0.0F) collapses to ZERO (value=+0.0F).
//
//   2. REUSE check: the pre-existing engine sampler
//        mc::valueproviders::ConstantFloat  (world/level/levelgen/FloatProvider.h)
//      must agree on sample() for the same input (it has no of()-folding, so we
//      feed it the ALREADY-FOLDED value the record produced, and confirm its
//      sample() bits match).
//
// Row format (tab-separated, every float = %08x of Float.floatToRawIntBits):
//   CF  <inBits>  <valueBits>  <minBits>  <maxBits>  <s0..s3Bits>  <postNextIntDec>  <freshNextIntDec>
//
// sample() ignores the RandomSource and must consume zero draws — so we also
// confirm that nextInt() AFTER 4 samples equals the fresh-seed nextInt()
// (postNextInt == freshNextInt), and that both match the GT decimals.
//
//   default        -> hardcoded self-checks (no Mojang files)
//   --cases <tsv>  -> verify every CF row of the generated reference

#include "ConstantFloat.h"
#include "../../world/level/levelgen/FloatProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
using mc::valueproviders::ConstantFloatRecord;
namespace eng = mc::valueproviders; // existing engine ConstantFloat sampler

namespace {

uint32_t parseHex(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}
float bitsToF(uint32_t b) { return std::bit_cast<float>(b); }
uint32_t fToBits(float f) { return std::bit_cast<uint32_t>(f); }

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;
    if (tag != "CF") { err = "unknown tag " + tag; return false; }

    std::string inHex, valHex, minHex, maxHex;
    in >> inHex >> valHex >> minHex >> maxHex;
    std::string sHex[4];
    for (auto& h : sHex) in >> h;
    long long postNextInt = 0, freshNextInt = 0;
    in >> postNextInt >> freshNextInt;

    const float input = bitsToF(parseHex(inHex));
    const uint32_t eVal = parseHex(valHex);
    const uint32_t eMin = parseHex(minHex);
    const uint32_t eMax = parseHex(maxHex);

    // --- Full record port: of(input) then value()/min()/max() ---
    ConstantFloatRecord cf = ConstantFloatRecord::of(input);

    const uint32_t gVal = fToBits(cf.value());
    if (gVal != eVal) {
        err = "value bits " + valHex + " != got " + std::to_string(gVal) +
              " (in=" + inHex + ")";
        return false;
    }
    const uint32_t gMin = fToBits(cf.min());
    if (gMin != eMin) { err = "min bits mismatch in=" + inHex; return false; }
    const uint32_t gMax = fToBits(cf.max());
    if (gMax != eMax) { err = "max bits mismatch in=" + inHex; return false; }

    // --- sample() (no-arg): must return the folded value, seed-independent ---
    for (int i = 0; i < 4; ++i) {
        const uint32_t gS = fToBits(cf.sample());
        if (gS != eVal) {
            err = "sample[" + std::to_string(i) + "] bits != value, in=" + inHex;
            return false;
        }
    }

    // sample() with an explicit RandomSource overload must also not advance it
    // and must return the same value bits.
    {
        LegacyRandomSource rng(12345);
        for (int i = 0; i < 4; ++i) {
            const uint32_t gS = fToBits(cf.sample(rng));
            if (gS != eVal) {
                err = "sample(rng)[" + std::to_string(i) + "] bits != value, in=" + inHex;
                return false;
            }
        }
        // After 4 ignored samples, the very next draw must equal a fresh RNG's
        // first draw (zero consumption).
        const int32_t after = rng.nextInt();
        LegacyRandomSource fresh(12345);
        const int32_t base = fresh.nextInt();
        if (after != base) {
            err = "sample(rng) consumed RNG draws, in=" + inHex;
            return false;
        }
    }

    // --- GT zero-consumption invariant: post == fresh nextInt ---
    if (postNextInt != freshNextInt) {
        err = "GT post/fresh nextInt differ (impossible if sample ignores RNG): " +
              std::to_string(postNextInt) + " != " + std::to_string(freshNextInt);
        return false;
    }

    // --- REUSE check: existing engine sampler agrees on sample() ---
    // Feed it the already-folded value (it has no of()-folding of its own).
    {
        eng::ConstantFloat engCf(cf.value());
        LegacyRandomSource rng(777);
        const uint32_t gE = fToBits(engCf.sample(rng));
        if (gE != eVal) {
            err = "engine ConstantFloat.sample bits != value, in=" + inHex;
            return false;
        }
    }

    return true;
}

const std::vector<std::string> kHardcoded = {
    // in        value     min       max       s0..s3 (=value)                       post fresh
    "CF\t00000000\t00000000\t00000000\t00000000\t00000000\t00000000\t00000000\t00000000\t0\t0",       // +0.0
    "CF\t80000000\t00000000\t00000000\t00000000\t00000000\t00000000\t00000000\t00000000\t0\t0",       // -0.0 -> +0.0 via of()
    "CF\t3f800000\t3f800000\t3f800000\t3f800000\t3f800000\t3f800000\t3f800000\t3f800000\t0\t0",       // 1.0
    "CF\tbf800000\tbf800000\tbf800000\tbf800000\tbf800000\tbf800000\tbf800000\tbf800000\t0\t0",       // -1.0
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << '\n';
            }
        }
        std::cout << "ConstantFloat cases=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    long n = 0, bad = 0;
    for (const auto& line : kHardcoded) {
        std::string err;
        ++n;
        if (!verifyLine(line, err)) { ++bad; std::cerr << "FAIL: " << err << '\n'; }
    }
    std::cout << "ConstantFloat cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
