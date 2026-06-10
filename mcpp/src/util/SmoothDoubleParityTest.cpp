// Parity test for net.minecraft.util.SmoothDouble (camera smoothing, STATEFUL).
// Ground truth: tools/SmoothDoubleParity.java vs the real class. The TSV is a stream
// of STEP / RESET rows tagged by sequence id; we keep one SmoothDouble per seq and
// replay the SAME calls in order, comparing the returned double AND the full internal
// state (targetValue/remainingValue/lastAmount) bit-for-bit (raw IEEE-754 bits only).
//
//   smooth_double_parity --cases mcpp/build/smooth_double.tsv

#include "util/SmoothDouble.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double   bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: smooth_double_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    std::map<int, mc::util::SmoothDouble> instances;  // one persistent instance per seq

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqB = [&](double got, const std::string& exp, const std::string& what, const std::string& l) {
        if (db(got) != std::stoull(exp, nullptr, 16))
            fail(l + " [" + what + "] gotbits=" + std::to_string(db(got)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "STEP") {
            // STEP seq targetDeltaBits timeBits retBits targetValueBits remainingValueBits lastAmountBits
            int seq = std::stoi(p[1]);
            double targetDelta = bd(p[2]);
            double time = bd(p[3]);
            auto& s = instances[seq];
            double ret = s.getNewDeltaValue(targetDelta, time);
            eqB(ret, p[4], "ret", line);
            eqB(s.targetValue(), p[5], "targetValue", line);
            eqB(s.remainingValue(), p[6], "remainingValue", line);
            eqB(s.lastAmount(), p[7], "lastAmount", line);
        } else if (t == "RESET") {
            // RESET seq targetValueBits remainingValueBits lastAmountBits
            int seq = std::stoi(p[1]);
            auto& s = instances[seq];
            s.reset();
            eqB(s.targetValue(), p[2], "targetValue", line);
            eqB(s.remainingValue(), p[3], "remainingValue", line);
            eqB(s.lastAmount(), p[4], "lastAmount", line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SmoothDouble cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
