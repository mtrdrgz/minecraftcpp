// Parity test VERIFYING the existing, already-certified C++ port
// mc::valueproviders::ClampedInt (in world/level/levelgen/IntProvider.h) against
// the REAL decompiled net.minecraft.util.valueproviders.ClampedInt.
//
// Ground truth: mcpp/tools/ClampedIntParity.java ->
//   tools/run_groundtruth.ps1 -Tool ClampedIntParity -Out mcpp/build/clamped_int.tsv
//
// ClampedInt.sample(random) = Mth.clamp(source.sample(random), min, max), with
// Mth.clamp(int) = Math.min(Math.max(value, min), max). We rebuild each wrapped
// source by name and seed a LegacyRandomSource identically (RandomSource::create),
// so the underlying draw sequence is bit-for-bit the Java one.
//
//   default        -> hardcoded smoke self-checks (no Mojang files)
//   --cases <tsv>  -> verify every SAMP/BOUND row of the generated reference
//
// Comparison is bit-for-bit via std::bit_cast<uint32_t> on the int results.

#include "../../world/level/levelgen/IntProvider.h"
#include "../../world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;
using namespace mc::valueproviders;

namespace {

// Reconstruct the exact ClampedInt cases authored in ClampedIntParity.java,
// keyed by the same names. The clamp window in the test TSV is also carried per
// row, but we build the providers here (window must match the Java definitions).
std::map<std::string, IntProviderPtr> buildCases() {
    std::map<std::string, IntProviderPtr> m;
    m["uni_-5_10__0_8"]     = ClampedInt::of(UniformInt::of(-5, 10), 0, 8);
    m["uni_-5_10__-3_12"]   = ClampedInt::of(UniformInt::of(-5, 10), -3, 12);
    m["uni_-5_10__-8_5"]    = ClampedInt::of(UniformInt::of(-5, 10), -8, 5);
    m["uni_-5_10__-20_20"]  = ClampedInt::of(UniformInt::of(-5, 10), -20, 20);
    m["uni_0_15__7_7"]      = ClampedInt::of(UniformInt::of(0, 15), 7, 7);
    m["uni_-100_100__-9_9"] = ClampedInt::of(UniformInt::of(-100, 100), -9, 9);
    m["const5__0_8"]        = ClampedInt::of(ConstantInt::of(5), 0, 8);
    m["const5__6_10"]       = ClampedInt::of(ConstantInt::of(5), 6, 10);
    m["const5__0_3"]        = ClampedInt::of(ConstantInt::of(5), 0, 3);
    m["bias0_20__0_5"]      = ClampedInt::of(BiasedToBottomInt::of(0, 20), 0, 5);
    m["bias0_20__3_18"]     = ClampedInt::of(BiasedToBottomInt::of(0, 20), 3, 18);
    m["bias-10_10__-2_2"]   = ClampedInt::of(BiasedToBottomInt::of(-10, 10), -2, 2);
    return m;
}

const auto g_cases = buildCases();

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

    if (tag == "SAMP") {
        // SAMP name seed min max s0..s7
        if (f.size() != 5 + 8) { err = "SAMP wrong field count"; return false; }
        const std::string& name = f[1];
        auto it = g_cases.find(name);
        if (it == g_cases.end()) { err = "unknown case " + name; return false; }
        int64_t seed = std::stoll(f[2]);
        auto rng = RandomSource::create(seed); // LegacyRandomSource, identical seed
        for (int i = 0; i < 8; i++) {
            int32_t expected = static_cast<int32_t>(std::stol(f[5 + i]));
            int32_t got = it->second->sample(*rng);
            if (!bitEqI32(got, expected)) {
                err = "SAMP " + name + " seed=" + f[2] + " idx=" + std::to_string(i)
                    + " got=" + std::to_string(got) + " exp=" + std::to_string(expected);
                return false;
            }
        }
        return true;
    }

    if (tag == "BOUND") {
        // BOUND name min max minInclusive maxInclusive
        if (f.size() != 6) { err = "BOUND wrong field count"; return false; }
        const std::string& name = f[1];
        auto it = g_cases.find(name);
        if (it == g_cases.end()) { err = "unknown case " + name; return false; }
        int32_t expMin = static_cast<int32_t>(std::stol(f[4]));
        int32_t expMax = static_cast<int32_t>(std::stol(f[5]));
        int32_t gotMin = it->second->minInclusive();
        int32_t gotMax = it->second->maxInclusive();
        if (!bitEqI32(gotMin, expMin)) {
            err = "BOUND " + name + " minInclusive got=" + std::to_string(gotMin)
                + " exp=" + std::to_string(expMin);
            return false;
        }
        if (!bitEqI32(gotMax, expMax)) {
            err = "BOUND " + name + " maxInclusive got=" + std::to_string(gotMax)
                + " exp=" + std::to_string(expMax);
            return false;
        }
        return true;
    }

    // Unknown tag: ignore (forward-compatible).
    return true;
}

int selfCheck() {
    // ClampedInt.sample = Mth.clamp(source.sample, min, max) =
    // min(max(v, lo), hi). ConstantInt makes the source deterministic.
    auto pinLow = ClampedInt::of(ConstantInt::of(5), 6, 10);   // 5 -> 6
    auto pinHigh = ClampedInt::of(ConstantInt::of(5), 0, 3);   // 5 -> 3
    auto pass = ClampedInt::of(ConstantInt::of(5), 0, 8);      // 5 -> 5
    auto rng = RandomSource::create(0);
    int mism = 0;
    if (pinLow->sample(*rng) != 6) { std::cerr << "selfcheck pinLow\n"; mism++; }
    if (pinHigh->sample(*rng) != 3) { std::cerr << "selfcheck pinHigh\n"; mism++; }
    if (pass->sample(*rng) != 5) { std::cerr << "selfcheck pass\n"; mism++; }
    // bounds: combined with wrapped source range
    auto cb = ClampedInt::of(UniformInt::of(-5, 10), 0, 8);
    if (cb->minInclusive() != 0) { std::cerr << "selfcheck min\n"; mism++; }   // max(0,-5)
    if (cb->maxInclusive() != 8) { std::cerr << "selfcheck max\n"; mism++; }   // min(8,10)
    auto cw = ClampedInt::of(UniformInt::of(-5, 10), -20, 20);
    if (cw->minInclusive() != -5) { std::cerr << "selfcheck minw\n"; mism++; } // max(-20,-5)
    if (cw->maxInclusive() != 10) { std::cerr << "selfcheck maxw\n"; mism++; } // min(20,10)
    std::cout << "ClampedInt selfcheck cases=8 mismatches=" << mism << "\n";
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

    std::cout << "ClampedInt cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
