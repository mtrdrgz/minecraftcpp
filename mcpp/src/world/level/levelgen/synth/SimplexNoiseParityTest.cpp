// Parity test for mc::levelgen::SimplexNoise (the existing engine port in
// world/level/levelgen/Noise.{h,cpp}) vs the REAL decompiled
// net.minecraft.world.level.levelgen.synth.SimplexNoise.
//
// This gate does NOT define a new SimplexNoise — it #includes the existing engine
// header and proves the certified code matches Java bit-for-bit. The ctor consumes
// a RandomSource (3 nextDouble + a 256-entry Fisher-Yates shuffle), so we rebuild
// each case with an identically seeded mc::levelgen::LegacyRandomSource — the same
// `new LegacyRandomSource(seed)` the Java GT tool uses.
//
// Ground truth: mcpp/tools/SimplexNoiseParity.java. Doubles compared by raw IEEE
// bits via std::bit_cast (never by value).
//
//   --cases <tsv>  -> verify every line of the generated reference
//   default        -> a couple of jar-free self-consistency checks

#include "Noise.h"
#include "RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::levelgen;

namespace {

double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream in(line);
    while (std::getline(in, cur, '\t')) out.push_back(cur);
    return out;
}

// Build the engine SimplexNoise with the same seeded RNG the Java GT used.
SimplexNoise makeNoise(int64_t seed) {
    LegacyRandomSource rng(seed);
    return SimplexNoise(rng);
}

} // namespace

int main(int argc, char** argv) {
    if (argc <= 2 || std::string(argv[1]) != "--cases") {
        // Self-test: ctor offsets are deterministic and 2D/3D values stay finite.
        SimplexNoise n = makeNoise(0);
        bool ok = (n.xo >= 0.0 && n.xo < 256.0) && (n.yo >= 0.0 && n.yo < 256.0) &&
                  (n.zo >= 0.0 && n.zo < 256.0);
        double a = n.getValue(0.5, -0.5);
        double b = n.getValue(0.5, -0.5, 1.5);
        ok = ok && (a == a) && (b == b); // not NaN
        std::cout << "SimplexNoise self-test " << (ok ? "passed" : "FAILED") << '\n';
        return ok ? 0 : 1;
    }

    std::ifstream f(argv[2]);
    if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }

    // Cache one SimplexNoise per seed (ctor consumes RNG; rebuild only on seed change).
    int64_t curSeed = 0;
    bool haveNoise = false;
    SimplexNoise noise = makeNoise(0);

    auto ensureSeed = [&](int64_t seed) {
        if (!haveNoise || seed != curSeed) {
            noise = makeNoise(seed);
            curSeed = seed;
            haveNoise = true;
        }
    };

    std::string line;
    long n = 0, bad = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        std::vector<std::string> t = split(line);
        if (t.empty()) continue;
        const std::string& tag = t[0];
        ++n;

        if (tag == "CTOR") {
            // CTOR seed xoBits yoBits zoBits
            int64_t seed = std::stoll(t[1]);
            ensureSeed(seed);
            uint64_t exo = std::stoull(t[2], nullptr, 16);
            uint64_t eyo = std::stoull(t[3], nullptr, 16);
            uint64_t ezo = std::stoull(t[4], nullptr, 16);
            if (db(noise.xo) != exo || db(noise.yo) != eyo || db(noise.zo) != ezo) {
                ++bad;
                if (bad <= 20)
                    std::cerr << "MISMATCH CTOR seed=" << seed
                              << " xo " << db(noise.xo) << "!=" << exo
                              << " yo " << db(noise.yo) << "!=" << eyo
                              << " zo " << db(noise.zo) << "!=" << ezo << '\n';
            }
        } else if (tag == "N2") {
            // N2 seed xBits yBits valueBits
            int64_t seed = std::stoll(t[1]);
            ensureSeed(seed);
            double x = bd(t[2]);
            double y = bd(t[3]);
            uint64_t ev = std::stoull(t[4], nullptr, 16);
            uint64_t gv = db(noise.getValue(x, y));
            if (gv != ev) {
                ++bad;
                if (bad <= 20)
                    std::cerr << "MISMATCH N2 seed=" << seed << " x=" << x << " y=" << y
                              << " got " << gv << " != " << ev << '\n';
            }
        } else if (tag == "N3") {
            // N3 seed xBits yBits zBits valueBits
            int64_t seed = std::stoll(t[1]);
            ensureSeed(seed);
            double x = bd(t[2]);
            double y = bd(t[3]);
            double z = bd(t[4]);
            uint64_t ev = std::stoull(t[5], nullptr, 16);
            uint64_t gv = db(noise.getValue(x, y, z));
            if (gv != ev) {
                ++bad;
                if (bad <= 20)
                    std::cerr << "MISMATCH N3 seed=" << seed << " x=" << x << " y=" << y
                              << " z=" << z << " got " << gv << " != " << ev << '\n';
            }
        } else {
            --n; // unknown tag, ignore
        }
    }

    std::cout << "SimplexNoise cases=" << n << " mismatches=" << bad << '\n';
    return bad == 0 ? 0 : 1;
}
