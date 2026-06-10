// Parity test for net.minecraft.core.GlobalPos (Minecraft 26.1.2).
// Ground truth: tools/GlobalPosParity.java vs the real decompiled classes.
// Integers/longs compared exactly; toString compared as exact raw strings.
//
//   global_pos_parity --cases mcpp/build/global_pos.tsv

#include "GlobalPos.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gp = mc::globalpos;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int       i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: global_pos_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != ll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqStr = [&](const std::string& got, const std::string& exp, const std::string& l) {
        if (got != exp) fail(l + " got=[" + got + "]");
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "GP_STR") {
            // ns path x y z  toString
            gp::GlobalPos g{ p[1], p[2], { i(p[3]), i(p[4]), i(p[5]) } };
            eqStr(g.toString(), p[6], line);
        } else if (t == "BP_ASLONG") {
            eqI(gp::blockPosAsLong(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        } else if (t == "BP_HASH") {
            eqI(gp::blockPosHashCode(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        } else if (t == "BP_CHESS") {
            eqI(gp::distChessboard(i(p[1]), i(p[2]), i(p[3]), i(p[4]), i(p[5]), i(p[6])), p[7], line);
        } else if (t == "GP_EQ") {
            gp::GlobalPos a{ p[1], p[2], { i(p[3]), i(p[4]), i(p[5]) } };
            gp::GlobalPos b{ p[6], p[7], { i(p[8]), i(p[9]), i(p[10]) } };
            eqI(a.equals(b) ? 1 : 0, p[11], line);
        } else if (t == "GP_CLOSE") {
            gp::GlobalPos a{ p[1], p[2], { i(p[3]), i(p[4]), i(p[5]) } };
            bool r = a.isCloseEnough(p[6], p[7], i(p[8]), i(p[9]), i(p[10]), i(p[11]));
            eqI(r ? 1 : 0, p[12], line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "GlobalPos cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
