// Parity test for net.minecraft.util.BlockUtil.getMaxRectangleLocation.
// Ground truth: tools/BlockUtilParity.java drives the REAL (package-private,
// @VisibleForTesting) helper by reflection and emits one TSV row per histogram.
// This test re-runs the 1:1 C++ port (util/BlockUtil.h) over the same int[] and
// compares the returned IntBounds(min,max) and height exactly (decimal ints).
//
//   block_util_parity --cases mcpp/build/block_util.tsv
//
// TSV row format (see GT tool header):
//   columnsCsv \t boundsMin \t boundsMax \t height
// columnsCsv = comma-separated int[] ("EMPTY" for length 0).

#include "BlockUtil.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> splitTab(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

std::vector<int32_t> parseCsvInts(const std::string& field) {
    if (field == "EMPTY" || field.empty()) return {};
    std::vector<int32_t> out;
    std::string it;
    std::istringstream ss(field);
    while (std::getline(ss, it, ',')) {
        out.push_back(static_cast<int32_t>(std::stoll(it)));
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) {
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: block_util_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = splitTab(line);
        if (p.size() < 4) continue;
        ++n;

        std::vector<int32_t> columns = parseCsvInts(p[0]);
        int32_t expMin = static_cast<int32_t>(std::stoll(p[1]));
        int32_t expMax = static_cast<int32_t>(std::stoll(p[2]));
        int32_t expHeight = static_cast<int32_t>(std::stoll(p[3]));

        mc::util::block_util::MaxRectangle got =
            mc::util::block_util::getMaxRectangleLocation(columns);

        bool bad = (got.bounds.min != expMin) || (got.bounds.max != expMax)
                   || (got.height != expHeight);
        if (bad) {
            ++mism;
            if (mism <= 20) {
                std::cerr << "MISMATCH columns=[" << p[0] << "] exp(min=" << expMin
                          << ",max=" << expMax << ",h=" << expHeight << ") got(min="
                          << got.bounds.min << ",max=" << got.bounds.max << ",h="
                          << got.height << ")\n";
            }
        }
    }

    std::cout << "BlockUtil checks=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
