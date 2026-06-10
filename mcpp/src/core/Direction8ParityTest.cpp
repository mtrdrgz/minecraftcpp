// Bit-exact parity gate for net.minecraft.core.Direction8 (Minecraft 26.1.2),
// ported in core/Direction8.h. Reads the TSV emitted by
// mcpp/tools/Direction8Parity.java and compares values exactly.
//
// Tags:
//   STEP  <d8 ord> <stepX> <stepY> <stepZ>
//   HAS   <d8 ord> <cardinal ord> <0|1>
//   COUNT <d8 ord> <count>
#include "core/Direction8.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a C++ Direction ordinal (DOWN=0..EAST=5) to mc::Direction.
static mc::Direction dirFromOrd(int o) { return mc::DIRECTION_VALUES[o]; }
// Map a C++ Direction8 ordinal (NORTH=0..NORTH_WEST=7) to mc::Direction8.
static mc::Direction8 dir8FromOrd(int o) { return mc::DIRECTION8_VALUES[o]; }

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: --cases <tsv>\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "STEP") {
            // <d8 ord> <stepX> <stepY> <stepZ>
            mc::Direction8 d = dir8FromOrd(std::stoi(p[1]));
            int expX = std::stoi(p[2]);
            int expY = std::stoi(p[3]);
            int expZ = std::stoi(p[4]);
            if (mc::direction8GetStepX(d) != expX) ++mism;
            else if (mc::direction8GetStepY(d) != expY) ++mism;
            else if (mc::direction8GetStepZ(d) != expZ) ++mism;
        } else if (tag == "HAS") {
            // <d8 ord> <cardinal ord> <0|1>
            mc::Direction8 d = dir8FromOrd(std::stoi(p[1]));
            mc::Direction c = dirFromOrd(std::stoi(p[2]));
            int exp = std::stoi(p[3]);
            int got = mc::direction8HasDirection(d, c) ? 1 : 0;
            if (got != exp) ++mism;
        } else if (tag == "COUNT") {
            // <d8 ord> <count>
            mc::Direction8 d = dir8FromOrd(std::stoi(p[1]));
            int exp = std::stoi(p[2]);
            if (mc::direction8PartCount(d) != exp) ++mism;
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++mism;
        }
    }

    std::cout << "Direction8 cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
