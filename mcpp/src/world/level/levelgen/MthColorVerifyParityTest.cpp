// mth_color_verify_parity — VERIFIES the colour/hash helpers already ported in
// world/level/levelgen/Mth.h (mc::levelgen::mth) bit-for-bit against ground truth
// from the REAL net.minecraft.util.Mth 26.1.2 (tools/MthColorVerifyParity.java):
//
//   hsvToRgb / hsvToArgb / murmurHash3Mixer / binarySearch /
//   frac(float) / frac(double) / smoothstep / smoothstepDerivative
//
// plus net.minecraft.util.ARGB.color(a,r,g,b) against a corrected local helper
// (MthColorVerifyHelpers.h) — this pins the & 0xFF channel masking that the engine
// header's argbColor() omits (faithful for the physical r/g/b/alpha ranges the gate
// uses, but the masked reference documents the full-range semantics).
//
// No new port of the verified methods: we #include Mth.h and call them directly.
// Floats/doubles are exchanged as raw IEEE-754 bit patterns, so the gate is exact.
//
//   mth_color_verify_parity --cases mcpp/build/mth_color_verify.tsv

#include "Mth.h"
#include "MthColorVerifyHelpers.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace mth = mc::levelgen::mth;
namespace cv  = mc::levelgen::mth_color_verify;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t u32(const std::string& s) { return static_cast<uint32_t>(std::stoul(s, nullptr, 16)); }
uint64_t u64(const std::string& s) { return std::stoull(s, nullptr, 16); }
float    bf(const std::string& s)  { return std::bit_cast<float>(u32(s)); }
double   bd(const std::string& s)  { return std::bit_cast<double>(u64(s)); }
uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
// Java prints ints/longs as signed decimal; parse with the wide signed reader.
long long ll(const std::string& s) { return std::stoll(s); }
int       i(const std::string& s)  { return static_cast<int>(std::stoll(s)); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: mth_color_verify_parity --cases <tsv>\n"; return 2; }
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
    auto chkF = [&](const std::string& t, const std::string& l, float got, const std::string& exp)  { if (fb(got) != u32(exp)) failF(t, l, fb(got), u32(exp)); };
    auto chkD = [&](const std::string& t, const std::string& l, double got, const std::string& exp) { if (db(got) != u64(exp)) failD(t, l, db(got), u64(exp)); };
    auto chkI = [&](const std::string& t, const std::string& l, long long got, const std::string& exp) { if (got != ll(exp)) failI(t, l, got, ll(exp)); };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "HSVRGB")        // hsvToRgb(hue,sat,val) -> packed (alpha 0)
            chkI(t, line, mth::hsvToRgb(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "HSVARGB")  // hsvToArgb(hue,sat,val,alpha)
            chkI(t, line, mth::hsvToArgb(bf(p[1]), bf(p[2]), bf(p[3]), i(p[4])), p[5]);
        else if (t == "ARGBCOLOR") // corrected masked ARGB.color(a,r,g,b)
            chkI(t, line, cv::argbColorMasked(i(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5]);
        else if (t == "MURMUR")
            chkI(t, line, mth::murmurHash3Mixer(i(p[1])), p[2]);
        else if (t == "BINSEARCH") {
            const int th = i(p[3]);
            chkI(t, line, mth::binarySearch(i(p[1]), i(p[2]), [th](int m){ return m >= th; }), p[4]);
        }
        else if (t == "FRAC_F")   chkF(t, line, mth::frac(bf(p[1])), p[2]);
        else if (t == "FRAC_D")   chkD(t, line, mth::frac(bd(p[1])), p[2]);
        else if (t == "SMOOTH")      chkD(t, line, mth::smoothstep(bd(p[1])), p[2]);
        else if (t == "SMOOTHDERIV") chkD(t, line, mth::smoothstepDerivative(bd(p[1])), p[2]);
        else { ++mism; if (shown++ < 40) std::cerr << "UNKNOWN_TAG " << t << "\n"; }
    }

    std::cout << "MthColorVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
