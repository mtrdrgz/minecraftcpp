// Bit-exact parity gate for net.minecraft.world.level.material.FogType
// (Minecraft 26.1.2), ported in world/level/material/FogType.h. Reads the TSV
// emitted by mcpp/tools/FogTypeParity.java and compares the constant count,
// ordinals and names exactly.
//
// Tags:
//   COUNT <values().length>
//   ENUM  <ordinal> <name>
#include "world/level/material/FogType.h"

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

        if (tag == "COUNT") {
            // <values().length>
            int exp = std::stoi(p[1]);
            if (mc::material::FOG_TYPE_COUNT != exp) ++mism;
        } else if (tag == "ENUM") {
            // <ordinal> <name>
            int ord = std::stoi(p[1]);
            const std::string& expName = p[2];
            // Ordinal must be in range, and the C++ value at that ordinal must
            // round-trip its ordinal and match the Java name exactly.
            if (ord < 0 || ord >= mc::material::FOG_TYPE_COUNT) {
                ++mism;
            } else {
                mc::material::FogType t = mc::material::fogTypeFromOrdinal(ord);
                if (mc::material::fogTypeOrdinal(t) != ord) {
                    ++mism;
                } else if (mc::material::fogTypeName(t) != expName) {
                    ++mism;
                }
            }
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++mism;
        }
    }

    std::cout << "FogType cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
