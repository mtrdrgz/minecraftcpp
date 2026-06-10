// Bit-exact parity GATE for net.minecraft.util.valueproviders.ClampedNormalFloat.
//
// VERIFY-EXISTING: the C++ port already lives in
//   world/level/levelgen/FloatProvider.h  (mc::valueproviders::ClampedNormalFloat)
// as  sample = mthClampF(mthNormal(r, mean, dev), min, max)
//         = clamp(mean + (float)r.nextGaussian() * dev, min, max),
// the verbatim translation of
//   Mth.clamp(Mth.normal(random, mean, deviation), min, max).
// This gate seeds an identical mc::levelgen::LegacyRandomSource and compares the
// raw IEEE-754 bits of sample() against ground truth produced by the REAL
// decompiled class (tools/ClampedNormalFloatParity.java).
//
//   --cases <tsv>  -> verify every CNF line of the generated reference.
//   (no args)      -> a few hardcoded self-checks so the binary is runnable solo.
//
// Row format (tab-separated, hex = Float.floatToRawIntBits):
//   CNF <meanBits> <devBits> <minBits> <maxBits> <seed> <count> <s0> <s1> ...

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
using mc::valueproviders::ClampedNormalFloat;

namespace {

float bitsToFloat(const std::string& hex) {
    // hex is up to 8 lowercase hex digits (no 0x), Float.floatToRawIntBits.
    const uint32_t bits = static_cast<uint32_t>(std::stoul(hex, nullptr, 16));
    return std::bit_cast<float>(bits);
}

uint32_t floatToBits(float v) { return std::bit_cast<uint32_t>(v); }

struct Stats {
    long cases = 0;
    long mismatches = 0;
};

bool verifyLine(const std::string& line, Stats& st, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;
    if (tag != "CNF") return true; // ignore foreign rows

    std::string meanHex, devHex, minHex, maxHex;
    long long seed = 0;
    int count = 0;
    in >> meanHex >> devHex >> minHex >> maxHex >> seed >> count;
    if (!in) { err = "malformed header"; return false; }

    const float mean = bitsToFloat(meanHex);
    const float dev = bitsToFloat(devHex);
    const float min = bitsToFloat(minHex);
    const float max = bitsToFloat(maxHex);

    ClampedNormalFloat provider(mean, dev, min, max);
    LegacyRandomSource r(static_cast<int64_t>(seed));

    for (int i = 0; i < count; ++i) {
        std::string expHex;
        if (!(in >> expHex)) { err = "short row at draw " + std::to_string(i); return false; }
        const uint32_t expected = static_cast<uint32_t>(std::stoul(expHex, nullptr, 16));
        const uint32_t got = floatToBits(provider.sample(r));
        ++st.cases;
        if (got != expected) {
            ++st.mismatches;
            if (st.mismatches <= 20) {
                std::ostringstream o;
                o << "CNF mean=" << mean << " dev=" << dev << " min=" << min << " max=" << max
                  << " seed=" << seed << " [" << i << "] got=0x" << std::hex << got
                  << " exp=0x" << expected;
                err = o.str();
                std::cerr << "MISMATCH: " << err << '\n';
            }
        }
    }
    return true;
}

// Self-checks (no Mojang files). These bit patterns come from the generated
// reference for the first config { 0.0, 1.0, -2.0, 2.0 } at seed 0 and 1.
// They double as a smoke test that LegacyRandomSource + the port are wired up.
const std::vector<std::string> kHardcoded = {
    // seed 0, config (0,1,-2,2): draw 0 == nextGaussian()*1 clamped, etc.
    // We only assert structural sanity here (no Mojang values baked in beyond
    // what the FloatProvider self-test already covers); the real gate is --cases.
};

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        Stats st;
        std::string line;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            if (!verifyLine(line, st, err)) {
                std::cerr << "PARSE FAIL: " << err << '\n';
                return 2;
            }
        }
        std::cout << "ClampedNormalFloat cases=" << st.cases
                  << " mismatches=" << st.mismatches << '\n';
        return st.mismatches == 0 ? 0 : 1;
    }

    // Solo smoke test: sample is deterministic for a fixed seed and stays within
    // [min,max]; deviation 0 collapses to clamp(mean) every draw.
    {
        ClampedNormalFloat degenerate(0.5f, 0.0f, 0.0f, 1.0f);
        LegacyRandomSource r(0);
        bool ok = true;
        for (int i = 0; i < 8; ++i) {
            const float v = degenerate.sample(r);
            if (floatToBits(v) != floatToBits(0.5f)) { ok = false; break; }
        }
        if (!ok) { std::cerr << "self-check FAILED: deviation-0 not constant\n"; return 1; }
    }
    {
        ClampedNormalFloat clamped(0.0f, 100.0f, -1.0f, 1.0f);
        LegacyRandomSource r(42);
        for (int i = 0; i < 64; ++i) {
            const float v = clamped.sample(r);
            if (v < -1.0f || v > 1.0f) {
                std::cerr << "self-check FAILED: sample out of [min,max]: " << v << '\n';
                return 1;
            }
        }
    }
    std::cout << "ClampedNormalFloat self-checks passed (run with --cases <tsv> for the full gate)\n";
    return 0;
}
