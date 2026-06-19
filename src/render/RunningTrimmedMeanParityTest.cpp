// Bit-exact parity gate for net.minecraft.client.renderer.RunningTrimmedMean
// (Minecraft 26.1.2). Reads the TSV emitted by tools/RunningTrimmedMeanParity.java
// and re-drives the C++ render/RunningTrimmedMean.h ring buffer for each scenario,
// comparing every returned trimmed-mean value exactly (decimal int64).
//
// TSV rows:
//   SEQ  <maxCount>  <n>  <in_0> <out_0> <in_1> <out_1> ... <in_{n-1}> <out_{n-1}>
//
//   mcpp/build/running_trimmed_mean_parity.exe --cases mcpp/build/running_trimmed_mean.tsv

#include "render/RunningTrimmedMean.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::render::RunningTrimmedMean;

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: running_trimmed_mean_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];
        if (tag != "SEQ") continue;  // forward-compatible: ignore unknown tags

        // p[1]=maxCount p[2]=n p[3..]=(in,out) pairs
        if (p.size() < 3) continue;
        int32_t maxCount = static_cast<int32_t>(std::stoll(p[1]));
        long n = std::stol(p[2]);
        if ((long)p.size() != 3 + 2 * n) {
            std::cerr << "MALFORMED row cap=" << maxCount << " n=" << n
                      << " cols=" << p.size() << "\n";
            ++cases;
            ++mism;
            continue;
        }

        RunningTrimmedMean m(maxCount);
        for (long k = 0; k < n; ++k) {
            ++cases;
            int64_t input = static_cast<int64_t>(std::stoll(p[3 + 2 * k]));
            int64_t expected = static_cast<int64_t>(std::stoll(p[3 + 2 * k + 1]));
            int64_t got = m.registerValueAndGetMean(input);
            if (got != expected) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH SEQ cap=" << maxCount << " step=" << k
                              << " in=" << input << " exp=" << expected
                              << " got=" << got << "\n";
                }
            }
        }
    }

    std::cout << "RunningTrimmedMean cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
