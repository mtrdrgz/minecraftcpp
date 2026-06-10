// Parity gate for the PURE (value-only) transform density functions of the engine
// DensityFunction port (world/level/levelgen/DensityFunction.h /.cpp) against the REAL
// net.minecraft.world.level.levelgen.DensityFunctions. These are the unary/binary
// functions whose output depends only on an input value (or blockY for the gradient),
// never on noise or world state:
//
//   Mapped:        abs, square, cube, half_negative, quarter_negative, squeeze, invert
//   Clamp(min,max)
//   RangeChoice(input, minInclusive, maxExclusive, whenInRange, whenOutOfRange)
//   YClampedGradient(fromY, toY, fromValue, toValue)
//   Constant(value)
//   add / mul / min / max of two Constants
//
// We rebuild the SAME nodes via the engine factory functions (DensityFunctions::map,
// clamp, rangeChoice, yClampedGradient, constant, add, mul, min, max) and drive each
// with the constant input from the TSV, comparing compute() bit-for-bit (std::bit_cast)
// to the Java row. Ground truth: tools/DensityFnsParity.java.
//
//   mcpp/tools/run_groundtruth.ps1 -Tool DensityFnsParity -Out mcpp/build/density_fns.tsv
//   density_fns_parity --cases mcpp/build/density_fns.tsv
//
// NOTE (engine bug FIXED, now certified by this gate): the engine's YClampedGradient
// used a local clampedMap that clamped t to [0,1] then returned toLow + t*(toHigh-toLow).
// Java's Mth.clampedMap = clampedLerp(inverseLerp(...)) returns the ENDPOINT VALUE
// directly when factor>1 / factor<0. For value pairs whose lerp endpoint does not
// reconstruct exactly (e.g. from=0.3,to=0.9: 0.3+(0.9-0.3) = 0.9000000000000001 != 0.9)
// the two differed by 1 ULP outside [fromY,toY]. DensityFunction.cpp's clampedMap now
// mirrors clampedLerp(inverseLerp(...)) exactly (endpoint short-circuit), so the YGRAD
// rows — which deliberately sweep y past toY/before fromY with non-round-tripping
// endpoints — are bit-exact.

#include "DensityFunction.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double   bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
int      pi(const std::string& s) { return std::stoi(s); }

DensityFunctionPtr K(double v) { return DensityFunctions::constant(v); }

const DensityFunctionContext CTX0{0, 0, 0};

DensityFunctions::MapType mapTypeFor(const std::string& tag) {
    if (tag == "MAP_ABS")              return DensityFunctions::MapType::Abs;
    if (tag == "MAP_SQUARE")           return DensityFunctions::MapType::Square;
    if (tag == "MAP_CUBE")             return DensityFunctions::MapType::Cube;
    if (tag == "MAP_HALF_NEGATIVE")    return DensityFunctions::MapType::HalfNegative;
    if (tag == "MAP_QUARTER_NEGATIVE") return DensityFunctions::MapType::QuarterNegative;
    if (tag == "MAP_SQUEEZE")          return DensityFunctions::MapType::Squeeze;
    if (tag == "MAP_INVERT")           return DensityFunctions::MapType::Invert;
    return DensityFunctions::MapType::Abs;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: density_fns_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        uint64_t got = 0, exp = 0;
        bool handled = true;

        if (tag.rfind("MAP_", 0) == 0) {
            // MAP_<NAME>\t<inputBits>\t<outBits>
            if (p.size() < 3) continue;
            double v = bd(p[1]);
            exp = std::stoull(p[2], nullptr, 16);
            auto f = DensityFunctions::map(K(v), mapTypeFor(tag));
            got = db(f->compute(CTX0));
        } else if (tag == "CLAMP") {
            // CLAMP\t<vBits>\t<minBits>\t<maxBits>\t<outBits>
            if (p.size() < 5) continue;
            double v = bd(p[1]), mn = bd(p[2]), mx = bd(p[3]);
            exp = std::stoull(p[4], nullptr, 16);
            auto f = DensityFunctions::clamp(K(v), mn, mx);
            got = db(f->compute(CTX0));
        } else if (tag == "RANGE") {
            // RANGE\t<vBits>\t<minInclBits>\t<maxExclBits>\t<outBits>
            if (p.size() < 5) continue;
            double v = bd(p[1]), mn = bd(p[2]), mx = bd(p[3]);
            exp = std::stoull(p[4], nullptr, 16);
            auto f = DensityFunctions::rangeChoice(K(v), mn, mx, K(7.0), K(-9.0));
            got = db(f->compute(CTX0));
        } else if (tag == "YGRAD") {
            // YGRAD\t<fromY>\t<toY>\t<fromValBits>\t<toValBits>\t<y>\t<outBits>
            if (p.size() < 7) continue;
            int fromY = pi(p[1]), toY = pi(p[2]);
            double fromVal = bd(p[3]), toVal = bd(p[4]);
            int y = pi(p[5]);
            exp = std::stoull(p[6], nullptr, 16);
            auto f = DensityFunctions::yClampedGradient(fromY, toY, fromVal, toVal);
            got = db(f->compute(DensityFunctionContext{0, y, 0}));
        } else if (tag == "CONST") {
            // CONST\t<vBits>\t<outBits>
            if (p.size() < 3) continue;
            double v = bd(p[1]);
            exp = std::stoull(p[2], nullptr, 16);
            auto f = DensityFunctions::constant(v);
            got = db(f->compute(CTX0));
        } else if (tag == "ADD" || tag == "MUL" || tag == "MIN" || tag == "MAX") {
            // <OP>\t<aBits>\t<bBits>\t<outBits>
            if (p.size() < 4) continue;
            double a = bd(p[1]), b = bd(p[2]);
            exp = std::stoull(p[3], nullptr, 16);
            DensityFunctionPtr f;
            if (tag == "ADD")      f = DensityFunctions::add(K(a), K(b));
            else if (tag == "MUL") f = DensityFunctions::mul(K(a), K(b));
            else if (tag == "MIN") f = DensityFunctions::min(K(a), K(b));
            else                   f = DensityFunctions::max(K(a), K(b));
            got = db(f->compute(CTX0));
        } else {
            handled = false;
        }

        if (!handled) continue;
        ++total;
        if (got != exp) {
            ++mism;
            if (shown++ < 40) {
                double g = std::bit_cast<double>(got), e = std::bit_cast<double>(exp);
                std::cerr << "MISMATCH " << tag << " got=" << g << " (" << std::hex << got
                          << ") exp=" << e << " (" << exp << std::dec << ") | " << line << "\n";
            }
        }
    }

    std::cout << "DensityFns cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
