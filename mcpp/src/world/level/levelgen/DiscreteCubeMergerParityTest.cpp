// Parity test for net.minecraft.world.phys.shapes.DiscreteCubeMerger.
//
// VERIFY-EXISTING: the C++ port already lives in world/phys/shapes/IndexMerger.h
// (class mc::DiscreteCubeMerger). This gate exercises that header against ground
// truth from tools/DiscreteCubeMergerParity.java (the REAL net.minecraft class) and
// compares BIT-FOR-BIT.
//
//   cube_voxel_parity --cases mcpp/build/cube_voxel.tsv
//
// DiscreteCubeMerger.java (Minecraft 26.1.2):
//   ctor(firstSize, secondSize):
//     result    = new CubePointRange((int)Shapes.lcm(firstSize, secondSize))
//                 Shapes.lcm(a,b) = (long)a * (b / IntMath.gcd(a,b))
//     gcd       = IntMath.gcd(firstSize, secondSize)
//     firstDiv  = firstSize / gcd
//     secondDiv = secondSize / gcd
//   size()              = result.size()  (= lcm + 1)
//   getList()           = result (a CubePointRange; getDouble(i) = (double)i / parts)
//   forMergedIndexes(c) : for i in [0, size-1): c.merge(i/secondDiv, i/firstDiv, i)
//
// Row formats (TAG \t inputs... \t outputs...):
//   CTOR   firstSize secondSize | gcd firstDiv secondDiv resultParts size
//   GET    firstSize secondSize index | getDouble(double, %016x raw bits of result list)
//   MERGE  firstSize secondSize | count  i:first:second  i:first:second  ...
//            (one token per merged index i in [0, size-1): "result:first:second")

#include "world/phys/shapes/IndexMerger.h"
#include "world/phys/shapes/JavaMath.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::DiscreteCubeMerger;
using mc::intMathGcd;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Java ints are signed 32-bit.
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Parse a %016x raw-bits hex token into the double it encodes.
double d_from_hex(const std::string& s) {
    uint64_t bits = std::stoull(s, nullptr, 16);
    return std::bit_cast<double>(bits);
}

// Parse "result:first:second" into three ints.
bool parseTriple(const std::string& tok, int32_t& r, int32_t& f, int32_t& s) {
    size_t c1 = tok.find(':');
    if (c1 == std::string::npos) return false;
    size_t c2 = tok.find(':', c1 + 1);
    if (c2 == std::string::npos) return false;
    r = static_cast<int32_t>(std::stoll(tok.substr(0, c1)));
    f = static_cast<int32_t>(std::stoll(tok.substr(c1 + 1, c2 - c1 - 1)));
    s = static_cast<int32_t>(std::stoll(tok.substr(c2 + 1)));
    return true;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: cube_voxel_parity --cases <tsv>\n";
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

        if (tag == "CTOR") {  // firstSize secondSize | gcd firstDiv secondDiv resultParts size
            int32_t firstSize = i(p[1]);
            int32_t secondSize = i(p[2]);
            DiscreteCubeMerger m(firstSize, secondSize);
            // Recompute the private divisors exactly as the Java ctor does.
            int32_t gcd = intMathGcd(firstSize, secondSize);
            int32_t firstDiv = firstSize / gcd;
            int32_t secondDiv = secondSize / gcd;
            // resultParts = size - 1 (CubePointRange.size() = parts + 1).
            int32_t size = m.size();
            int32_t resultParts = size - 1;
            bad = gcd != i(p[3]) || firstDiv != i(p[4]) || secondDiv != i(p[5])
                  || resultParts != i(p[6]) || size != i(p[7]);
        } else if (tag == "GET") {  // firstSize secondSize index | getDouble (raw bits)
            int32_t firstSize = i(p[1]);
            int32_t secondSize = i(p[2]);
            int32_t index = i(p[3]);
            DiscreteCubeMerger m(firstSize, secondSize);
            double got = m.getList()->getDouble(index);
            double exp = d_from_hex(p[4]);
            bad = std::bit_cast<uint64_t>(got) != std::bit_cast<uint64_t>(exp);
        } else if (tag == "MERGE") {  // firstSize secondSize | count  result:first:second ...
            int32_t firstSize = i(p[1]);
            int32_t secondSize = i(p[2]);
            int32_t count = i(p[3]);
            DiscreteCubeMerger m(firstSize, secondSize);

            // Capture the C++ forMergedIndexes output (first, second, result).
            std::vector<std::array<int32_t, 3>> got;  // {result, first, second}
            m.forMergedIndexes([&got](int32_t first, int32_t second, int32_t result) {
                got.push_back({result, first, second});
                return true;
            });

            if (static_cast<int32_t>(got.size()) != count) {
                bad = true;
            } else {
                // Each remaining token p[4..] is "result:first:second".
                for (int32_t k = 0; k < count; ++k) {
                    int32_t er, ef, es;
                    if (!parseTriple(p[4 + k], er, ef, es)) { bad = true; break; }
                    if (got[k][0] != er || got[k][1] != ef || got[k][2] != es) {
                        bad = true;
                        break;
                    }
                }
            }
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

    std::cout << "DiscreteCubeMerger cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
