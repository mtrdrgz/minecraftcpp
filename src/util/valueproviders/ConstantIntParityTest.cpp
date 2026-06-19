// Parity test VERIFYING the existing, already-certified C++ port
// mc::valueproviders::ConstantInt (in world/level/levelgen/IntProvider.h) against
// the REAL decompiled net.minecraft.util.valueproviders.ConstantInt.
//
// Ground truth: mcpp/tools/ConstantIntParity.java ->
//   tools/run_groundtruth.ps1 -Tool ConstantIntParity -Out mcpp/build/int_provider_constant.tsv
//
// ConstantInt(value): sample(random) = value (PURE — ignores the rng),
// minInclusive() = maxInclusive() = value, and of(0) returns a ZERO singleton.
// The C++ port has no singleton identity to observe, so the BOUND check verifies
// minInclusive/maxInclusive only (the isZeroSingleton column is informational and
// just confirms minInclusive == maxInclusive == 0 for value 0). SAMP rows prove the
// C++ sample() returns `value` for all 8 draws regardless of the rng state the Java
// side perturbed between samples.
//
//   default        -> hardcoded smoke self-checks (no Mojang files)
//   --cases <tsv>  -> verify every BOUND/SAMP row of the generated reference
//
// Comparison is bit-for-bit via std::bit_cast<uint32_t> on the int results.

#include "../../world/level/levelgen/IntProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::valueproviders;

namespace {

bool bitEqI32(int32_t a, int32_t b) {
    return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
}

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::string tok;
    std::istringstream in(line);
    while (std::getline(in, tok, delim)) out.push_back(tok);
    return out;
}

bool verifyLine(const std::string& line, std::string& err) {
    if (line.empty()) return true;
    auto f = split(line, '\t');
    if (f.empty()) return true;
    const std::string& tag = f[0];

    if (tag == "BOUND") {
        // BOUND value minInclusive maxInclusive isZeroSingleton
        if (f.size() != 5) { err = "BOUND wrong field count"; return false; }
        int32_t value = static_cast<int32_t>(std::stol(f[1]));
        int32_t expMin = static_cast<int32_t>(std::stol(f[2]));
        int32_t expMax = static_cast<int32_t>(std::stol(f[3]));
        auto c = ConstantInt::of(value);
        int32_t gotMin = c->minInclusive();
        int32_t gotMax = c->maxInclusive();
        if (!bitEqI32(gotMin, expMin)) {
            err = "BOUND value=" + f[1] + " minInclusive got=" + std::to_string(gotMin)
                + " exp=" + std::to_string(expMin);
            return false;
        }
        if (!bitEqI32(gotMax, expMax)) {
            err = "BOUND value=" + f[1] + " maxInclusive got=" + std::to_string(gotMax)
                + " exp=" + std::to_string(expMax);
            return false;
        }
        // The port stores a single value; minInclusive == maxInclusive == value is the
        // ConstantInt invariant. (Java's of(0)->ZERO singleton has no C++ analogue.)
        if (!bitEqI32(gotMin, value) || !bitEqI32(gotMax, value)) {
            err = "BOUND value=" + f[1] + " bounds != value";
            return false;
        }
        return true;
    }

    if (tag == "SAMP") {
        // SAMP value rng seed s0..s7
        if (f.size() != 4 + 8) { err = "SAMP wrong field count"; return false; }
        int32_t value = static_cast<int32_t>(std::stol(f[1]));
        int64_t seed = std::stoll(f[3]);
        auto c = ConstantInt::of(value);
        // sample() is pure; any RandomSource works. Use LegacyRandomSource seeded
        // identically and perturb it the same way the Java GT did, to prove our
        // sample() likewise ignores rng state.
        auto rng = RandomSource::create(seed);
        for (int i = 0; i < 8; i++) {
            int32_t expected = static_cast<int32_t>(std::stol(f[4 + i]));
            int32_t got = c->sample(*rng);
            if (!bitEqI32(got, expected)) {
                err = "SAMP value=" + f[1] + " rng=" + f[2] + " seed=" + f[3]
                    + " idx=" + std::to_string(i) + " got=" + std::to_string(got)
                    + " exp=" + std::to_string(expected);
                return false;
            }
            rng->nextInt(); // mirror the GT's between-sample perturbation
        }
        return true;
    }

    // Unknown tag: ignore (forward-compatible).
    return true;
}

int selfCheck() {
    int mism = 0;
    auto rng = RandomSource::create(123456789);
    // of(v): bounds == v, sample == v for any rng draws.
    for (int32_t v : {0, 1, -1, 7, 127, -128, 1000000, 2147483647, -2147483647 - 1}) {
        auto c = ConstantInt::of(v);
        if (!bitEqI32(c->minInclusive(), v)) { std::cerr << "selfcheck min v=" << v << "\n"; mism++; }
        if (!bitEqI32(c->maxInclusive(), v)) { std::cerr << "selfcheck max v=" << v << "\n"; mism++; }
        for (int i = 0; i < 8; i++) {
            if (!bitEqI32(c->sample(*rng), v)) { std::cerr << "selfcheck sample v=" << v << "\n"; mism++; }
            rng->nextInt();
        }
    }
    std::cout << "ConstantInt selfcheck cases=9 mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    std::string tsv;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) tsv = argv[++i];
    }

    if (tsv.empty()) {
        return selfCheck();
    }

    std::ifstream in(tsv);
    if (!in) {
        std::cerr << "cannot open " << tsv << "\n";
        return 2;
    }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::string err;
        cases++;
        if (!verifyLine(line, err)) {
            if (mismatches < 20) std::cerr << "MISMATCH: " << err << "\n";
            mismatches++;
        }
    }

    std::cout << "ConstantInt cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
