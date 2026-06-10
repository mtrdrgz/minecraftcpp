// Parity test for mc::Pose (world/entity/Pose.h) vs Java ground truth.
// Reads the TSV emitted by PoseParity.java and compares value-for-value.
#include "world/entity/Pose.h"

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
        std::cerr << "usage: PoseParityTest --cases <tsv>\n";
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
            // CONST <ordinal> <id> <name> <getSerializedName>
            ++cases;
            int ord = std::stoi(p[1]);
            int id = std::stoi(p[2]);
            const std::string& name = p[3];
            const std::string& serName = p[4];

            auto v = static_cast<mc::Pose>(ord);
            bool ok = true;
            if (mc::poseOrdinal(v) != ord) ok = false;
            if (mc::poseId(v) != id) ok = false;
            if (name != std::string(mc::poseName(v))) ok = false;
            if (serName != std::string(mc::poseSerializedName(v))) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord
                          << " id=" << id
                          << " name=" << name
                          << " serName=" << serName << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "Pose cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
