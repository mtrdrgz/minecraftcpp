// Parity test for mc::HumanoidArm (world/entity/HumanoidArm.h) vs Java ground truth.
// Reads the TSV emitted by HumanoidArmParity.java and compares value-for-value.
#include "world/entity/HumanoidArm.h"

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
        std::cerr << "usage: HumanoidArmParityTest --cases <tsv>\n";
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
            // CONST <ordinal> <name> <serializedName> <id> <opposite.ordinal>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& nm = p[2];
            const std::string& serialized = p[3];
            int id = std::stoi(p[4]);
            int oppOrd = std::stoi(p[5]);

            auto v = static_cast<mc::HumanoidArm>(ord);
            bool ok = true;
            if (mc::ordinal(v) != ord) ok = false;
            if (nm != std::string(mc::name(v))) ok = false;
            if (serialized != std::string(mc::getSerializedName(v))) ok = false;
            if (mc::humanoidArmId(v) != id) ok = false;
            if (mc::ordinal(mc::getOpposite(v)) != oppOrd) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord << " name=" << nm
                          << " serialized=" << serialized << " id=" << id
                          << " opposite=" << oppOrd << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "HumanoidArm cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
