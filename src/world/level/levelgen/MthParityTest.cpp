// Parity test for the full net.minecraft.util.Mth surface. Ground truth generated
// by tools/MthParity.java against the real 26.1.2 class. Each row carries inputs +
// expected output; floats/doubles are exchanged as raw IEEE-754 bit patterns, so
// the gate is bit-exact.
//
//   mth_parity --cases mcpp/build/mth.tsv

#include "Mth.h"

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
    if (casesPath.empty()) { std::cerr << "usage: mth_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    std::string line;
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

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "SIN")            chkF(t, line, mth::sin(bd(p[1])), p[2]);
        else if (t == "COS")       chkF(t, line, mth::cos(bd(p[1])), p[2]);
        else if (t == "SQRTF")     chkF(t, line, mth::sqrt(bf(p[1])), p[2]);
        else if (t == "FLOOR_D")   chkI(t, line, mth::floor(bd(p[1])), p[2]);
        else if (t == "FLOOR_F")   chkI(t, line, mth::floor(bf(p[1])), p[2]);
        else if (t == "LFLOOR")    chkI(t, line, mth::lfloor(bd(p[1])), p[2]);
        else if (t == "CEIL_D")    chkI(t, line, mth::ceil(bd(p[1])), p[2]);
        else if (t == "CEIL_F")    chkI(t, line, mth::ceil(bf(p[1])), p[2]);
        else if (t == "CEILLONG")  chkI(t, line, mth::ceilLong(bd(p[1])), p[2]);
        else if (t == "ABS_F")     chkF(t, line, mth::abs(bf(p[1])), p[2]);
        else if (t == "ABS_I")     chkI(t, line, mth::abs(i(p[1])), p[2]);
        else if (t == "FRAC_F")    chkF(t, line, mth::frac(bf(p[1])), p[2]);
        else if (t == "FRAC_D")    chkD(t, line, mth::frac(bd(p[1])), p[2]);
        else if (t == "WRAPDEG_I") chkI(t, line, mth::wrapDegrees(i(p[1])), p[2]);
        else if (t == "WRAPDEG_L") chkF(t, line, mth::wrapDegrees(static_cast<int64_t>(ll(p[1]))), p[2]);
        else if (t == "WRAPDEG_F") chkF(t, line, mth::wrapDegrees(bf(p[1])), p[2]);
        else if (t == "WRAPDEG_D") chkD(t, line, mth::wrapDegrees(bd(p[1])), p[2]);
        else if (t == "SMOOTH")    chkD(t, line, mth::smoothstep(bd(p[1])), p[2]);
        else if (t == "SMOOTHDERIV") chkD(t, line, mth::smoothstepDerivative(bd(p[1])), p[2]);
        else if (t == "SIGN")      chkI(t, line, mth::sign(bd(p[1])), p[2]);
        else if (t == "SQUARE_D")  chkD(t, line, mth::square(bd(p[1])), p[2]);
        else if (t == "SQUARE_F")  chkF(t, line, mth::square(bf(p[1])), p[2]);
        else if (t == "SQUARE_I")  chkI(t, line, mth::square(i(p[1])), p[2]);
        else if (t == "SQUARE_L")  chkI(t, line, mth::square(static_cast<int64_t>(ll(p[1]))), p[2]);
        else if (t == "CUBE_F")    chkF(t, line, mth::cube(bf(p[1])), p[2]);
        else if (t == "FASTINVSQRT")  chkD(t, line, mth::fastInvSqrt(bd(p[1])), p[2]);
        else if (t == "FASTINVCBRT_F") chkF(t, line, mth::fastInvCubeRoot(bf(p[1])), p[2]);
        else if (t == "INVSQRT_F") chkF(t, line, mth::invSqrt(bf(p[1])), p[2]);
        else if (t == "INVSQRT_D") chkD(t, line, mth::invSqrt(bd(p[1])), p[2]);
        else if (t == "PACKDEG")   chkI(t, line, mth::packDegrees(bf(p[1])), p[2]);
        else if (t == "UNPACKDEG") chkF(t, line, mth::unpackDegrees(static_cast<int8_t>(i(p[1]))), p[2]);
        else if (t == "SEPOT")     chkI(t, line, mth::smallestEncompassingPowerOfTwo(i(p[1])), p[2]);
        else if (t == "ISPOW2")    chkI(t, line, mth::isPowerOfTwo(i(p[1])) ? 1 : 0, p[2]);
        else if (t == "CEILLOG2")  chkI(t, line, mth::ceillog2(i(p[1])), p[2]);
        else if (t == "LOG2")      chkI(t, line, mth::log2(i(p[1])), p[2]);
        else if (t == "SQSIDE")    chkI(t, line, mth::smallestSquareSide(i(p[1])), p[2]);
        else if (t == "MURMUR")    chkI(t, line, mth::murmurHash3Mixer(i(p[1])), p[2]);
        else if (t == "CLAMP_D")   chkD(t, line, mth::clamp(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "CLAMP_F")   chkF(t, line, mth::clamp(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "CLAMPEDLERP_D") chkD(t, line, mth::clampedLerpD(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "CLAMPEDLERP_F") chkF(t, line, mth::clampedLerpF(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "INVLERP_D") chkD(t, line, mth::inverseLerp(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "INVLERP_F") chkF(t, line, mth::inverseLerp(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "MAP_D")     chkD(t, line, mth::map(bd(p[1]), bd(p[2]), bd(p[3]), bd(p[4]), bd(p[5])), p[6]);
        else if (t == "MAP_F")     chkF(t, line, mth::map(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4]), bf(p[5])), p[6]);
        else if (t == "CLAMPEDMAP_D") chkD(t, line, mth::clampedMapD(bd(p[1]), bd(p[2]), bd(p[3]), bd(p[4]), bd(p[5])), p[6]);
        else if (t == "QUANTIZE")  chkI(t, line, mth::quantize(bd(p[1]), i(p[2])), p[3]);
        else if (t == "FLOORDIV")  chkI(t, line, mth::floorDiv(i(p[1]), i(p[2])), p[3]);
        else if (t == "POSMOD_I")  chkI(t, line, mth::positiveModulo(i(p[1]), i(p[2])), p[3]);
        else if (t == "POSMOD_F")  chkF(t, line, mth::positiveModulo(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "POSMOD_D")  chkD(t, line, mth::positiveModulo(bd(p[1]), bd(p[2])), p[3]);
        else if (t == "POSCEILDIV") chkI(t, line, mth::positiveCeilDiv(i(p[1]), i(p[2])), p[3]);
        else if (t == "ROUNDTOWARD") chkI(t, line, mth::roundToward(i(p[1]), i(p[2])), p[3]);
        else if (t == "ATAN2")     chkD(t, line, mth::atan2(bd(p[1]), bd(p[2])), p[3]);
        else if (t == "HSV")       chkI(t, line, mth::hsvToArgb(bf(p[1]), bf(p[2]), bf(p[3]), i(p[4])), p[5]);
        else if (t == "LERP2")     chkD(t, line, mth::lerp2(bd(p[1]), bd(p[2]), bd(p[3]), bd(p[4]), bd(p[5]), bd(p[6])), p[7]);
        else if (t == "LERP3")     chkD(t, line, mth::lerp3(bd(p[1]), bd(p[2]), bd(p[3]), bd(p[4]), bd(p[5]), bd(p[6]), bd(p[7]), bd(p[8]), bd(p[9]), bd(p[10]), bd(p[11])), p[12]);
        else if (t == "CATMULL")   chkF(t, line, mth::catmullrom(bf(p[1]), bf(p[2]), bf(p[3]), bf(p[4]), bf(p[5])), p[6]);
        else if (t == "ROTLERP_F") chkF(t, line, mth::rotLerp(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "ROTLERPRAD") chkF(t, line, mth::rotLerpRad(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "TRIWAVE")   chkF(t, line, mth::triangleWave(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "LEN2_D")    chkD(t, line, mth::length(bd(p[1]), bd(p[2])), p[3]);
        else if (t == "LEN3_D")    chkD(t, line, mth::length(bd(p[1]), bd(p[2]), bd(p[3])), p[4]);
        else if (t == "LENSQ3_F")  chkF(t, line, mth::lengthSquared(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "LERPINT")   chkI(t, line, mth::lerpInt(bf(p[1]), i(p[2]), i(p[3])), p[4]);
        else if (t == "LERPDISCRETE") chkI(t, line, mth::lerpDiscrete(bf(p[1]), i(p[2]), i(p[3])), p[4]);
        else if (t == "GETSEED")   chkI(t, line, mth::getSeed(i(p[1]), i(p[2]), i(p[3])), p[4]);
        else if (t == "DEGDIFF")   chkF(t, line, mth::degreesDifference(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "DEGDIFFABS") chkF(t, line, mth::degreesDifferenceAbs(bf(p[1]), bf(p[2])), p[3]);
        else if (t == "ROTIFNEC")  chkF(t, line, mth::rotateIfNecessary(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "APPROACH")  chkF(t, line, mth::approach(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "APPROACHDEG") chkF(t, line, mth::approachDegrees(bf(p[1]), bf(p[2]), bf(p[3])), p[4]);
        else if (t == "EQUAL_F")   chkI(t, line, mth::equal(bf(p[1]), bf(p[2])) ? 1 : 0, p[3]);
        else if (t == "EQUAL_D")   chkI(t, line, mth::equal(bd(p[1]), bd(p[2])) ? 1 : 0, p[3]);
        else if (t == "CHESS")     chkI(t, line, mth::chessboardDistance(i(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5]);
        else if (t == "ISMULT")    chkI(t, line, mth::isMultipleOf(i(p[1]), i(p[2])) ? 1 : 0, p[3]);
        else if (t == "ABSMAX_I")  chkI(t, line, mth::absMax(i(p[1]), i(p[2])), p[3]);
        else if (t == "BINSEARCH") { int th = i(p[3]); chkI(t, line, mth::binarySearch(i(p[1]), i(p[2]), [th](int m){ return m >= th; }), p[4]); }
        // Verify the embedded atan2 LUTs (MthAtanTables.inc) match the Java dump bit-for-bit.
        else if (t == "ASINTAB") { uint64_t g = mth::MTH_ASIN_TAB_BITS[i(p[1])]; if (g != u64(p[2])) failD(t, line, g, u64(p[2])); }
        else if (t == "COSTAB")  { uint64_t g = mth::MTH_COS_TAB_BITS[i(p[1])]; if (g != u64(p[2])) failD(t, line, g, u64(p[2])); }
        else { ++mism; if (shown++ < 40) std::cerr << "UNKNOWN_TAG " << t << "\n"; }
    }

    std::cout << "Mth cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
