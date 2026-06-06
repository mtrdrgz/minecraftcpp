// Parity test for the C++ NoiseRouter / RandomState wiring against real Java.
//
// Ground truth: tools/DensityParity.java (the real decompiled RandomState).
//   mcpp/tools/run_groundtruth.ps1 -Tool DensityParity -Out build/density_cases.tsv
//   density_parity --cases build/density_cases.tsv
//
// Each TSV row: seed \t x \t y \t z \t function \t rawDoubleBits.
// Samples the RAW router functions (pre NoiseChunk cacheAllInCell) so it matches
// Java's rs.router().<fn>().compute(SinglePointContext), validating audit #6
// (RandomState mapAll / NoiseWiringHelper / seed-wired density graph).

#include "NoiseGeneratorSettings.h"
#include "RandomState.h"
#include "NoiseRouterData.h"
#include "DensityFunction.h"

#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

// The block registry is irrelevant to density-function values (defaultBlock only
// affects block placement, not the noise router). Stub it so this parity target
// need not link the whole Blocks registry.
namespace mc {
std::uint32_t getDefaultBlockStateId(std::string_view, std::uint32_t fallback) { return fallback; }
}

using namespace mc::levelgen;

namespace {
struct Case { long long seed; int x, y, z; std::string fn; std::uint64_t bits; };

std::uint64_t rawBits(double v) { std::uint64_t b; std::memcpy(&b, &v, 8); return b; }

DensityFunctionPtr pick(const NoiseRouter& r, const std::string& n) {
    if (n == "temperature")   return r.temperature;
    if (n == "vegetation")    return r.vegetation;
    if (n == "continents")    return r.continents;
    if (n == "erosion")       return r.erosion;
    if (n == "depth")         return r.depth;
    if (n == "ridges")        return r.ridges;
    if (n == "final_density") return r.finalDensity;
    return nullptr;
}
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: density_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    std::vector<Case> cases;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        Case c; std::string seedStr, bitsStr;
        std::getline(ss, seedStr, '\t');
        std::string xs, ys, zs;
        std::getline(ss, xs, '\t'); std::getline(ss, ys, '\t'); std::getline(ss, zs, '\t');
        std::getline(ss, c.fn, '\t'); std::getline(ss, bitsStr, '\t');
        if (bitsStr.empty()) continue;
        c.seed = std::strtoll(seedStr.c_str(), nullptr, 10);
        c.x = std::atoi(xs.c_str()); c.y = std::atoi(ys.c_str()); c.z = std::atoi(zs.c_str());
        c.bits = static_cast<std::uint64_t>(std::strtoll(bitsStr.c_str(), nullptr, 10));
        cases.push_back(std::move(c));
    }

    // Build the router once per distinct seed (in first-seen order).
    long long curSeed = 0; bool have = false;
    NoiseRouter router;
    int total = 0, mismatch = 0, shown = 0;
    for (const Case& c : cases) {
        if (!have || c.seed != curSeed) {
            RandomState rs(NoiseGeneratorSettings::overworld(), static_cast<std::uint64_t>(c.seed));
            router = NoiseRouterData::overworld(rs, false, false);
            curSeed = c.seed; have = true;
        }
        DensityFunctionPtr fn = pick(router, c.fn);
        if (!fn) { std::cerr << "unknown function: " << c.fn << "\n"; return 2; }
        const double v = fn->compute(DensityFunctionContext{ c.x, c.y, c.z });
        const std::uint64_t got = rawBits(v);
        ++total;
        if (got != c.bits) {
            ++mismatch;
            if (shown < 12) {
                double exp; std::uint64_t eb = c.bits; std::memcpy(&exp, &eb, 8);
                std::cerr << "MISMATCH seed=" << c.seed << " (" << c.x << "," << c.y << "," << c.z << ") "
                          << c.fn << "  got=" << v << " expected=" << exp << "\n";
                ++shown;
            }
        }
    }
    std::cout << "DensityRouter cases=" << total << " mismatches=" << mismatch << "\n";
    return mismatch == 0 ? 0 : 1;
}
