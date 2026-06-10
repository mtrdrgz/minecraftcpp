// Parity test for net.minecraft.world.phys.shapes.CubePointRange.
//
// VERIFY-EXISTING: the C++ port already lives in world/phys/shapes/DoubleList.h
// (class mc::CubePointRange). This gate exercises that header against ground truth
// from tools/CubePointRangeParity.java (the REAL net.minecraft class) and compares
// BIT-FOR-BIT.
//
//   cube_point_range_parity --cases mcpp/build/cube_point_range.tsv
//
// CubePointRange.java (Minecraft 26.1.2):
//   ctor(int parts): throws if parts <= 0; the C++ ctor treats parts >= 1 as a hard
//                    invariant, so CTOR here just checks the >0 predicate (no exception
//                    is modeled in C++).
//   getDouble(int index) = (double)index / parts
//   size()              = parts + 1
//
// Row formats (TAG \t inputs... \t outputs...):
//   CTOR   parts | constructible(0/1)
//   SIZE   parts | size(int, decimal)
//   GET    parts index | getDouble(double, %016x raw bits)

#include "world/phys/shapes/DoubleList.h"

#include <cstdint>
#include <cstdio>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::CubePointRange;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Java ints are signed 32-bit (MIN_VALUE prints as -2147483648, MAX as 2147483647).
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Parse a %016x raw-bits hex token into the double it encodes.
double d_from_hex(const std::string& s) {
    uint64_t bits = std::stoull(s, nullptr, 16);
    return std::bit_cast<double>(bits);
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: cube_point_range_parity --cases <tsv>\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "CTOR") {  // parts | constructible(0/1)
            int32_t parts = i(p[1]);
            // CubePointRange.java:9 — constructible iff parts > 0.
            int got = (parts > 0) ? 1 : 0;
            bad = got != i(p[2]);
        } else if (tag == "SIZE") {  // parts | size
            CubePointRange r(i(p[1]));
            bad = r.size() != i(p[2]);
        } else if (tag == "GET") {  // parts index | getDouble (raw bits)
            CubePointRange r(i(p[1]));
            double got = r.getDouble(i(p[2]));
            double exp = d_from_hex(p[3]);
            // Bit-for-bit: compare raw IEEE-754 bit patterns, not numeric ==.
            bad = std::bit_cast<uint64_t>(got) != std::bit_cast<uint64_t>(exp);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "CubePointRange cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
