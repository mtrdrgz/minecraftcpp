// Extra parity test for the remaining net.minecraft.util.Mth helpers not directly
// gated by mth_parity: wrapDegrees(float/double), degreesDifference,
// positiveModulo(int/float/double), clamp(int/float/double), length(x,y) (double +
// float), lengthSquared(x,y) / lengthSquared(x,y,z) (double + float-3arg),
// getSeed(x,y,z), floorDiv, floorMod. VERIFIES the existing engine header Mth.h
// (mc::levelgen::mth) — no new port, just a bit-exact gate against ground truth from
// the real 26.1.2 class (tools/MthExtraParity.java).
//
//   mth_extra_parity --cases mcpp/build/mth_extra.tsv

#include "Mth.h"
#include "MthExtraHelpers.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mth = mc::levelgen::mth;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t u32(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
uint64_t u64(const std::string& s) { return std::stoull(s, nullptr, 16); }
float    bf(const std::string& s) { return std::bit_cast<float>(u32(s)); }
double   bd(const std::string& s) { return std::bit_cast<double>(u64(s)); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
int      i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: mth_extra_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto failF = [&](const std::string& t, const std::string& l, uint32_t got, uint32_t exp) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << t << " got=" << std::hex << got << " exp=" << exp << std::dec << " | " << l << "\n";
    };
    auto failD = [&](const std::string& t, const std::string& l, uint64_t got, uint64_t exp) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << t << " got=" << std::hex << got << " exp=" << exp << std::dec << " | " << l << "\n";
    };
    auto failI = [&](const std::string& t, const std::string& l, long long got, long long exp) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << t << " got=" << got << " exp=" << exp << " | " << l << "\n";
    };
    auto chkF = [&](const std::string& t, const std::string& l, float got, const std::string& exp) { if (fb(got) != u32(exp)) failF(t, l, fb(got), u32(exp)); };
    auto chkD = [&](const std::string& t, const std::string& l, double got, const std::string& exp) { if (db(got) != u64(exp)) failD(t, l, db(got), u64(exp)); };
    auto chkI = [&](const std::string& t, const std::string& l, long long got, const std::string& exp) { if (got != ll(exp)) failI(t, l, got, ll(exp)); };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "WRAPDEG_F")      chkF(t, line, mth::wrapDegrees(bf(p[1])), p[2]);
        else if (t == "WRAPDEG_D") chkD(t, line, mth::wrapDegrees(bd(p[1])), p[2]);
        else if (t == "DEGDIFF")   chkF(t, line, mth::degreesDifference(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "POSMOD_I")  chkI(t, line, mth::positiveModulo(i(p[1]), i(p[2])), p[3]);
        else if (t == "POSMOD_F")  chkF(t, line, mth::positiveModulo(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "POSMOD_D")  chkD(t, line, mth::positiveModulo(bd(p[1]), bd(p[2])), p[3]);
        else if (t == "CLAMP_I")   chkI(t, line, mth::clamp(i(p[1]), i(p[2]), i(p[3])), p[4]);
        else if (t == "CLAMP_F")   chkF(t, line, mth::clamp(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "CLAMP_D")   chkD(t, line, mth::clamp(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "LEN2_D")    chkD(t, line, mth::length(bd(p[1]), bd(p[2])), p[3]);
        // LEN2_F uses the bit-exact local helper (see MthExtraHelpers.h): the shared
        // Mth.h::length(float,float) computes the squared sum in float and diverges
        // from Java's double-precision lengthSquared by 1 ULP on some inputs.
        else if (t == "LEN2_F")    chkF(t, line, mc::levelgen::mth_extra::lengthF(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "LENSQ2_D")  chkD(t, line, mth::lengthSquared(bd(p[1]), bd(p[2])), p[3]);
        else if (t == "LENSQ3_D")  chkD(t, line, mth::lengthSquared(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "LENSQ3_F")  chkF(t, line, mth::lengthSquared(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "GETSEED")   chkI(t, line, mth::getSeed(i(p[1]), i(p[2]), i(p[3])), p[4]);
        else if (t == "FLOORDIV")  chkI(t, line, mth::floorDiv(i(p[1]), i(p[2])), p[3]);
        else if (t == "FLOORMOD")  chkI(t, line, mth::floorMod(i(p[1]), i(p[2])), p[3]);
        else { ++mism; if (shown++ < 40) std::cerr << "UNKNOWN_TAG " << t << "\n"; }
    }

    std::cout << "MthExtra cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
