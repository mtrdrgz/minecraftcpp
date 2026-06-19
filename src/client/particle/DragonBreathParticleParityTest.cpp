// Parity test for mcpp/src/client/particle/DragonBreathParticle.h — the pure per-tick
// motion integrator + getQuadSize of net.minecraft.client.particle.DragonBreathParticle
// (Minecraft 26.1.2).
//
// Ground truth: tools/DragonBreathParticleParity.java, which allocateInstance()s a REAL
// DragonBreathParticle (no constructor -> no RandomSource, no ClientLevel), plugs in a
// null-returning SpriteSet proxy so setSpriteFromAge is a harmless sprite swap, reflectively
// seeds its state, and drives the REAL tick()/getQuadSize() bodies, emitting the resulting
// state. Each row carries the inputs + expected output columns; here we replay the same op
// sequence through DragonBreathParticle and compare BIT-FOR-BIT.
//
//   dragon_breath_particle_parity --cases mcpp/build/dragon_breath_particle.tsv

#include "DragonBreathParticle.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cp = mc::client::particle;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double   bd(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16))); }
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }

// Seed pos + bb the way the Java driver (Particle.setPos) does.
void seedPos(cp::DragonBreathParticle& s, double x, double y, double z) {
    s.x = x; s.y = y; s.z = z;
    float w = 0.6F / 2.0F;
    float h = 1.8F;
    s.bb = cp::DBAABB::make(x - static_cast<double>(w), y, z - static_cast<double>(w),
                            x + static_cast<double>(w), y + static_cast<double>(h), z + static_cast<double>(w));
    s.xo = x; s.yo = y; s.zo = z;
}

// Compare the live C++ state against the 14 expected state columns beginning at index `base`.
int compareState(const cp::DragonBreathParticle& s, const std::vector<std::string>& p,
                 std::size_t base, const std::string& tag, long long mismatches) {
    struct DChk { const char* name; uint64_t got; };
    struct IChk { const char* name; int got; };

    const DChk dchecks[] = {
        {"xo", db(s.xo)}, {"yo", db(s.yo)}, {"zo", db(s.zo)},
        {"x", db(s.x)},   {"y", db(s.y)},   {"z", db(s.z)},
        {"xd", db(s.xd)}, {"yd", db(s.yd)}, {"zd", db(s.zd)},
    };
    const IChk ichecks[] = {
        {"age", s.age},
        {"onGround", s.onGround ? 1 : 0},
        {"hasHitGround", s.hasHitGround ? 1 : 0},
        {"stopped", s.stoppedByCollision ? 1 : 0},
        {"removed", s.removed ? 1 : 0},
    };

    int bad = 0;
    std::size_t col = base;
    for (const auto& c : dchecks) {
        uint64_t expect = static_cast<uint64_t>(std::stoull(p[col++], nullptr, 16));
        if (c.got != expect) {
            ++bad;
            if (mismatches + bad <= 20)
                std::cerr << tag << " field=" << c.name << " expect=" << std::hex << expect
                          << " got=" << c.got << std::dec << "\n";
        }
    }
    for (const auto& c : ichecks) {
        int expect = std::stoi(p[col++]);
        if (c.got != expect) {
            ++bad;
            if (mismatches + bad <= 20)
                std::cerr << tag << " field=" << c.name << " expect=" << expect
                          << " got=" << c.got << "\n";
        }
    }
    return bad;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: dragon_breath_particle_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "TICK") {
            // TICK x y z xd yd zd life onGround hasHitGround stopped ticks | <14 state>
            if (p.size() < 12 + 14) continue;
            double x = bd(p[1]), y = bd(p[2]), z = bd(p[3]);
            double xd = bd(p[4]), yd = bd(p[5]), zd = bd(p[6]);
            int life = std::stoi(p[7]);
            bool onGround = std::stoi(p[8]) != 0;
            bool hitGround = std::stoi(p[9]) != 0;
            bool stopped = std::stoi(p[10]) != 0;
            int ticks = std::stoi(p[11]);

            cp::DragonBreathParticle s;
            seedPos(s, x, y, z);
            s.xd = xd; s.yd = yd; s.zd = zd;
            s.lifetime = life;
            s.onGround = onGround;
            s.hasHitGround = hitGround;
            s.stoppedByCollision = stopped;
            for (int t = 0; t < ticks; ++t) s.tick();

            int bad = compareState(s, p, 12, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "QSIZE") {
            // QSIZE quadSize age lifetime aBits | quadSizeBits
            if (p.size() < 6) continue;
            float qs = bf(p[1]);
            int age = std::stoi(p[2]);
            int life = std::stoi(p[3]);
            float a = bf(p[4]);
            uint32_t expect = static_cast<uint32_t>(std::stoul(p[5], nullptr, 16));

            cp::DragonBreathParticle s;
            s.quadSize = qs;
            s.age = age;
            s.lifetime = life;
            float got = s.getQuadSize(a);

            ++checks;
            if (fb(got) != expect) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "QSIZE field=quadSize expect=" << std::hex << expect
                              << " got=" << fb(got) << std::dec << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "DragonBreathParticle checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
