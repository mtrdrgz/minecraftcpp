// Parity test for mc::MobCategory (world/entity/MobCategory.h) vs Java ground truth.
// Reads the TSV emitted by MobCategoryParity.java and compares value-for-value.
#include "world/entity/MobCategory.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split_tabs(const std::string& line) {
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
        std::cerr << "usage: MobCategoryParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <getName> <getSerializedName> <max> <isFriendly>
            //       <isPersistent> <despawnDistance> <noDespawnDistance>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            const std::string& serName = p[3];
            int maxInst = std::stoi(p[4]);
            int isFriendly = std::stoi(p[5]);
            int isPersistent = std::stoi(p[6]);
            int despawn = std::stoi(p[7]);
            int noDespawn = std::stoi(p[8]);

            auto v = static_cast<mc::MobCategory>(ord);
            bool ok = true;
            if (name != std::string(mc::mobCategoryGetName(v))) ok = false;
            if (serName != std::string(mc::mobCategorySerializedName(v))) ok = false;
            if (mc::mobCategoryGetMaxInstancesPerChunk(v) != maxInst) ok = false;
            if (static_cast<int>(mc::mobCategoryIsFriendly(v)) != isFriendly) ok = false;
            if (static_cast<int>(mc::mobCategoryIsPersistent(v)) != isPersistent) ok = false;
            if (mc::mobCategoryGetDespawnDistance(v) != despawn) ok = false;
            if (mc::mobCategoryGetNoDespawnDistance(v) != noDespawn) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord
                          << " name=" << name
                          << " serName=" << serName
                          << " max=" << maxInst
                          << " isFriendly=" << isFriendly
                          << " isPersistent=" << isPersistent
                          << " despawn=" << despawn
                          << " noDespawn=" << noDespawn << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "MobCategory cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
