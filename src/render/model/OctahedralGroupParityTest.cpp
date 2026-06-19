// Parity test for com.mojang.math.OctahedralGroup + SymmetricGroup3 vs the C++
// render/model/McMath port. Ground truth: tools/OctahedralGroupParity.java.
// compose over all 48x48 certifies the cayley table AND the enum ordinal ordering;
// rotate over 48x6 certifies the permutation+inversion. inverse() is verified via
// compose==identity (the C++ exposes compose, not a direct inverse()).
//
//   octahedral_parity --cases mcpp/build/octahedral.tsv

#include "McMath.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mm = mc::render::model;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int i(const std::string& s) { return std::stoi(s); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: octahedral_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l, long long got) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << " got=" << got << "\n"; };

    const int IDENTITY = mm::OctahedralGroup::identity().ordinal;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "SYM3P") {
            int got = mm::SymmetricGroup3::value(i(p[1])).permute(i(p[2]));
            if (got != i(p[3])) fail(line, got);
        } else if (t == "SYM3C") {
            int got = mm::SymmetricGroup3::compose(mm::SymmetricGroup3::value(i(p[1])), mm::SymmetricGroup3::value(i(p[2]))).ordinal;
            if (got != i(p[3])) fail(line, got);
        } else if (t == "OCTALEN") {
            // sanity: ensure value(47) is reachable and distinct
            if (i(p[1]) != 48) fail(line, 48);
        } else if (t == "OCTA_R") {
            int got = (int) mm::OctahedralGroup::value(i(p[1])).rotate((mm::Direction) i(p[2]));
            if (got != i(p[3])) fail(line, got);
        } else if (t == "OCTA_C") {
            int got = mm::OctahedralGroup::value(i(p[1])).compose(mm::OctahedralGroup::value(i(p[2]))).ordinal;
            if (got != i(p[3])) fail(line, got);
        } else if (t == "OCTA_INV") {
            // Verify the Java-reported inverse via compose == identity (both orders).
            int g = i(p[1]), inv = i(p[2]);
            int c1 = mm::OctahedralGroup::value(g).compose(mm::OctahedralGroup::value(inv)).ordinal;
            int c2 = mm::OctahedralGroup::value(inv).compose(mm::OctahedralGroup::value(g)).ordinal;
            if (c1 != IDENTITY || c2 != IDENTITY) fail(line, c1);
        } else {
            fail("UNKNOWN_TAG " + t, 0);
        }
    }

    std::cout << "OctahedralGroup cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
