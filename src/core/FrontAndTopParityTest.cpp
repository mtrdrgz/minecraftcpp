// Parity test for mc::FrontAndTop (core/FrontAndTop.h) vs Java ground truth.
// Reads the TSV emitted by FrontAndTopParity.java and compares bit/value-for-value.
#include "core/FrontAndTop.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

// (kept for convention/parity-harness uniformity; FrontAndTop carries no floats)
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }

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
        std::cerr << "usage: FrontAndTopParityTest --cases <tsv>\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name> <front.ord> <top.ord>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            int frontOrd = std::stoi(p[3]);
            int topOrd = std::stoi(p[4]);
            auto v = static_cast<mc::FrontAndTop>(ord);
            bool ok = true;
            if (name != std::string(mc::frontAndTopSerializedName(v))) ok = false;
            if (static_cast<int>(mc::frontAndTopFront(v)) != frontOrd) ok = false;
            if (static_cast<int>(mc::frontAndTopTop(v)) != topOrd) ok = false;
            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord << " name=" << name
                          << " front=" << frontOrd << " top=" << topOrd << "\n";
            }
        } else if (tag == "LOOKUP") {
            // LOOKUP <front.ord> <top.ord> <key>
            ++cases;
            int frontOrd = std::stoi(p[1]);
            int topOrd = std::stoi(p[2]);
            int key = std::stoi(p[3]);
            int got = mc::frontAndTopLookupKey(static_cast<mc::Direction>(frontOrd),
                                               static_cast<mc::Direction>(topOrd));
            if (got != key) {
                ++mism;
                std::cerr << "LOOKUP mismatch front=" << frontOrd << " top=" << topOrd
                          << " expected=" << key << " got=" << got << "\n";
            }
        } else if (tag == "FROM") {
            // FROM <front.ord> <top.ord> <resultOrdinal | -1>
            ++cases;
            int frontOrd = std::stoi(p[1]);
            int topOrd = std::stoi(p[2]);
            int expect = std::stoi(p[3]);
            int got = mc::frontAndTopFromFrontAndTopOrdinal(static_cast<mc::Direction>(frontOrd),
                                                            static_cast<mc::Direction>(topOrd));
            if (got != expect) {
                ++mism;
                std::cerr << "FROM mismatch front=" << frontOrd << " top=" << topOrd
                          << " expected=" << expect << " got=" << got << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "FrontAndTop cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
