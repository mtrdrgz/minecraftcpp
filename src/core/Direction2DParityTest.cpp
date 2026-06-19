// Bit-exact parity gate for the 2D / yaw helpers of net.minecraft.core.Direction
// (Minecraft 26.1.2), ported in core/Direction2D.h. Reads the TSV emitted by
// mcpp/tools/Direction2DParity.java and compares BIT-FOR-BIT.
#include "core/Direction2D.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
static uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Map a C++ ordinal (DOWN=0..EAST=5) to mc::Direction.
static mc::Direction dirFromOrd(int o) {
    return mc::DIRECTION_VALUES[o];
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

        if (tag == "GET2D") {
            // <ord> <expected get2DDataValue>
            mc::Direction d = dirFromOrd(std::stoi(p[1]));
            int got = mc::direction2DGet2DDataValue(d);
            int exp = std::stoi(p[2]);
            if (got != exp) ++mism;
        } else if (tag == "TOYROT") {
            // <ord> <toYRot bits>
            mc::Direction d = dirFromOrd(std::stoi(p[1]));
            uint32_t got = fb(mc::direction2DToYRot(d));
            uint32_t exp = fb(bf(p[2]));
            if (got != exp) ++mism;
        } else if (tag == "GETYROT") {
            // <ord> <getYRot bits>
            mc::Direction d = dirFromOrd(std::stoi(p[1]));
            uint32_t got = fb(mc::direction2DGetYRot(d));
            uint32_t exp = fb(bf(p[2]));
            if (got != exp) ++mism;
        } else if (tag == "FROM2D") {
            // <data> <expected ord>
            int data = (int)std::stol(p[1]);
            int got = (int)mc::direction2DFrom2DDataValue(data);
            int exp = std::stoi(p[2]);
            if (got != exp) ++mism;
        } else if (tag == "FROMYROT") {
            // <yRot bits> <expected ord>
            double y = bd(p[1]);
            int got = (int)mc::direction2DFromYRot(y);
            int exp = std::stoi(p[2]);
            if (got != exp) ++mism;
        } else if (tag == "OPP2D") {
            // <ord> <expected opposite ord>
            mc::Direction d = dirFromOrd(std::stoi(p[1]));
            int got = (int)mc::direction2DGetOpposite(d);
            int exp = std::stoi(p[2]);
            if (got != exp) ++mism;
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++mism;
        }
    }

    std::cout << "Direction2D cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
