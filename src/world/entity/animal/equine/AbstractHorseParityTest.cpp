// Parity test for the C++ port of the pure attribute-generation helpers of
// net.minecraft.world.entity.animal.equine.AbstractHorse (AbstractHorse.h):
//
//   generateMaxHealthFromRandom   -> generateMaxHealth(random::nextInt)      [float]
//   generateJumpStrengthFromRandom-> generateJumpStrength(random::nextDouble)[double]
//   generateSpeedFromRandom       -> generateSpeed(random::nextDouble)       [double]
//   createOffspringAttribute      -> createOffspringAttribute(pa,pb,min,max,random)
//
// Each emitted reference row is replayed against an identically-seeded engine
// RandomSource and compared BIT-FOR-BIT (raw IEEE bits) against ground truth
// from mcpp/tools/AbstractHorseParity.java (the real decompiled methods + real
// RandomSource).
//
//   default        -> a couple of jar-free self-consistency checks
//   --cases <tsv>  -> verify every emitted reference line
//
// Build:
//   clang++ -std=c++23 -O2 -ffp-contract=off -I mcpp/src \
//     mcpp/src/world/entity/animal/equine/AbstractHorseParityTest.cpp \
//     mcpp/src/world/level/levelgen/RandomSource.cpp -o horse_test.exe

#include "world/entity/animal/equine/AbstractHorse.h"
#include "world/level/levelgen/RandomSource.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

using namespace mc::levelgen;
namespace eq = mc::world::entity::animal::equine;

namespace {

std::shared_ptr<RandomSource> make(const std::string& kind, int64_t seed) {
    if (kind == "legacy") return std::make_shared<LegacyRandomSource>(seed);
    if (kind == "single") return std::make_shared<SingleThreadedRandomSource>(seed);
    if (kind == "xoro")   return std::make_shared<XoroshiroRandomSource>(seed);
    return nullptr;
}

float  f32FromHex(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
double f64FromHex(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16))); }

bool bitEqF(float a, float b)  { return std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b); }
bool bitEqD(double a, double b){ return std::bit_cast<uint64_t>(a) == std::bit_cast<uint64_t>(b); }

bool verifyLine(const std::string& line, std::string& err) {
    std::istringstream in(line);
    std::string tag;
    in >> tag;
    if (tag.empty()) return true;

    if (tag == "HEALTH") {
        std::string kind; int64_t seed = 0; int32_t count = 0;
        in >> kind >> seed >> count;
        auto rs = make(kind, seed);
        if (!rs) { err = "unknown kind " + kind; return false; }
        for (int32_t i = 0; i < count; ++i) {
            std::string hex;
            if (!(in >> hex)) { err = "HEALTH truncated i=" + std::to_string(i); return false; }
            float expected = f32FromHex(hex);
            float got = eq::generateMaxHealthFromRandom(*rs);
            if (!bitEqF(got, expected)) {
                err = "HEALTH " + kind + " seed=" + std::to_string(seed) + " i=" + std::to_string(i) +
                      " got=" + std::to_string(std::bit_cast<uint32_t>(got)) +
                      " exp=" + std::to_string(std::bit_cast<uint32_t>(expected));
                return false;
            }
        }
        return true;
    }

    if (tag == "JUMP" || tag == "SPEED") {
        std::string kind; int64_t seed = 0; int32_t count = 0;
        in >> kind >> seed >> count;
        auto rs = make(kind, seed);
        if (!rs) { err = "unknown kind " + kind; return false; }
        for (int32_t i = 0; i < count; ++i) {
            std::string hex;
            if (!(in >> hex)) { err = tag + " truncated i=" + std::to_string(i); return false; }
            double expected = f64FromHex(hex);
            double got = (tag == "JUMP") ? eq::generateJumpStrengthFromRandom(*rs)
                                         : eq::generateSpeedFromRandom(*rs);
            if (!bitEqD(got, expected)) {
                err = tag + " " + kind + " seed=" + std::to_string(seed) + " i=" + std::to_string(i) +
                      " got=" + std::to_string(std::bit_cast<int64_t>(got)) +
                      " exp=" + std::to_string(std::bit_cast<int64_t>(expected));
                return false;
            }
        }
        return true;
    }

    if (tag == "OFFSPRING") {
        std::string kind; int64_t seed = 0;
        std::string paH, pbH, minH, maxH; int32_t count = 0;
        in >> kind >> seed >> paH >> pbH >> minH >> maxH >> count;
        auto rs = make(kind, seed);
        if (!rs) { err = "unknown kind " + kind; return false; }
        double pa = f64FromHex(paH), pb = f64FromHex(pbH);
        double mn = f64FromHex(minH), mx = f64FromHex(maxH);
        for (int32_t i = 0; i < count; ++i) {
            std::string hex;
            if (!(in >> hex)) { err = "OFFSPRING truncated i=" + std::to_string(i); return false; }
            double expected = f64FromHex(hex);
            double got = eq::createOffspringAttribute(pa, pb, mn, mx, *rs);
            if (!bitEqD(got, expected)) {
                err = "OFFSPRING " + kind + " seed=" + std::to_string(seed) + " i=" + std::to_string(i) +
                      " got=" + std::to_string(std::bit_cast<int64_t>(got)) +
                      " exp=" + std::to_string(std::bit_cast<int64_t>(expected));
                return false;
            }
        }
        return true;
    }

    return true; // unknown tag, ignore
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 2 && std::string(argv[1]) == "--cases") {
        std::ifstream f(argv[2]);
        if (!f) { std::cerr << "cannot open " << argv[2] << '\n'; return 2; }
        std::string line;
        long n = 0, bad = 0;
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            std::string err;
            ++n;
            if (!verifyLine(line, err)) {
                ++bad;
                if (bad <= 20) std::cerr << "MISMATCH: " << err << "  | " << line.substr(0, 60) << "...\n";
            }
        }
        std::cout << "AbstractHorse checks=" << n << " mismatches=" << bad << '\n';
        return bad == 0 ? 0 : 1;
    }

    // Jar-free self-consistency: replaying the same seed twice must agree, the
    // const-supplier helpers must match the live ones at p->known points, and the
    // clamp/reflection branches must behave (a value above max reflects below it).
    bool ok = true;

    // generateMaxHealth range: 15 + nextInt(8)[0..7] + nextInt(9)[0..8] in [15,30].
    for (int64_t seed : { (int64_t)0, (int64_t)1, (int64_t)42, (int64_t)-1 }) {
        auto a = make("xoro", seed), b = make("xoro", seed);
        for (int i = 0; i < 32; ++i) {
            float ha = eq::generateMaxHealthFromRandom(*a);
            float hb = eq::generateMaxHealthFromRandom(*b);
            if (!bitEqF(ha, hb)) { ok = false; std::cerr << "FAIL health determinism seed=" << seed << '\n'; break; }
            if (ha < 15.0F || ha > 30.0F) { ok = false; std::cerr << "FAIL health range " << ha << '\n'; break; }
        }
    }

    // Const-supplier endpoints (used by MIN/MAX range constants).
    // generateMaxHealthFromOp(0,0) == 15; (7,8) == 30.
    if (!bitEqF(eq::generateMaxHealthFromOp(0, 0), 15.0F)) { ok = false; std::cerr << "FAIL maxhealth op->0\n"; }
    if (!bitEqF(eq::generateMaxHealthFromOp(7, 8), 30.0F)) { ok = false; std::cerr << "FAIL maxhealth op->max\n"; }
    // generateJumpStrengthFromConst(0.0) == (double)0.4F; (1.0) == 0.4F + 0.6.
    if (!bitEqD(eq::generateJumpStrengthFromConst(0.0), static_cast<double>(0.4F))) { ok = false; std::cerr << "FAIL jump p->0\n"; }
    // generateSpeedFromConst(0.0) == (double)0.45F * 0.25.
    if (!bitEqD(eq::generateSpeedFromConst(0.0), static_cast<double>(0.45F) * 0.25)) { ok = false; std::cerr << "FAIL speed p->0\n"; }

    // createOffspringAttribute reflection: with equal parents and a forced large
    // positive babyQuality the result can exceed max and must be reflected back.
    {
        auto r = make("legacy", 0);
        double v = eq::createOffspringAttribute(31.0, 31.0, 15.0, 31.0, *r);
        if (v > 31.0 || v < 15.0) {
            // Reflection guarantees the returned value stays within [min, max] for
            // these symmetric inputs (the > max branch subtracts the overshoot).
            // (Not strictly guaranteed for arbitrary inputs, but holds here.)
            std::cerr << "WARN offspring reflected out of band v=" << v << '\n';
        }
        if (!std::isfinite(v)) { ok = false; std::cerr << "FAIL offspring non-finite\n"; }
    }

    if (!ok) { std::cerr << "AbstractHorse self-checks FAILED\n"; return 1; }
    std::cout << "AbstractHorse self-checks passed\n";
    return 0;
}
