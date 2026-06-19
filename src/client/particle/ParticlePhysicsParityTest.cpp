// Parity test for mcpp/src/client/particle/ParticlePhysics.h — the pure
// (collision-free, hasPhysics == false) physics integrator of the particle base class
// net.minecraft.client.particle.Particle (Minecraft 26.1.2).
//
// Ground truth: tools/ParticlePhysicsParity.java, which allocateInstance()s a REAL
// Particle (no constructor -> no RandomSource, no ClientLevel) and reflectively drives
// its real setSize / setPos / move / tick / scale / setPower bodies, emitting the full
// resulting integrator state. Each row carries the inputs + the 21 state columns; here we
// replay the same op sequence through ParticlePhysics and compare BIT-FOR-BIT.
//
//   particle_physics_parity --cases mcpp/build/particle_physics.tsv

#include "ParticlePhysics.h"

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

// Compare the live C++ integrator state against the 21 expected state columns that begin
// at index `base` in the row `p`. Returns the number of mismatched fields (0 == bit-exact)
// and, on first failure, prints a diagnostic.
int compareState(const cp::ParticlePhysics& s, const std::vector<std::string>& p,
                 std::size_t base, const std::string& tag, long long mismatches) {
    struct DChk { const char* name; uint64_t got; };
    struct FChk { const char* name; uint32_t got; };
    struct IChk { const char* name; int got; };

    const DChk dchecks[] = {
        {"xo", db(s.xo)}, {"yo", db(s.yo)}, {"zo", db(s.zo)},
        {"x", db(s.x)},   {"y", db(s.y)},   {"z", db(s.z)},
        {"xd", db(s.xd)}, {"yd", db(s.yd)}, {"zd", db(s.zd)},
        {"minX", db(s.bb.minX)}, {"minY", db(s.bb.minY)}, {"minZ", db(s.bb.minZ)},
        {"maxX", db(s.bb.maxX)}, {"maxY", db(s.bb.maxY)}, {"maxZ", db(s.bb.maxZ)},
    };
    const FChk fchecks[] = {
        {"bbWidth", fb(s.bbWidth)}, {"bbHeight", fb(s.bbHeight)},
    };
    const IChk ichecks[] = {
        {"age", s.age}, {"onGround", s.onGround ? 1 : 0},
        {"stopped", s.stoppedByCollision ? 1 : 0}, {"removed", s.removed ? 1 : 0},
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
    for (const auto& c : fchecks) {
        uint32_t expect = static_cast<uint32_t>(std::stoul(p[col++], nullptr, 16));
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
    if (casesPath.empty()) { std::cerr << "usage: particle_physics_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        cp::ParticlePhysics s;          // defaults mirror Particle.java
        s.hasPhysics = false;           // hermetic regime (matches the GT tool)

        if (tag == "TICK") {
            // TICK x y z xd yd zd grav frict bbW bbH life ticks su | <21 state>
            if (p.size() < 14 + 21) continue;
            double x = bd(p[1]), y = bd(p[2]), z = bd(p[3]);
            double xd = bd(p[4]), yd = bd(p[5]), zd = bd(p[6]);
            float grav = bf(p[7]), frict = bf(p[8]);
            float bbW = bf(p[9]), bbH = bf(p[10]);
            int life = std::stoi(p[11]);
            int ticks = std::stoi(p[12]);
            int su = std::stoi(p[13]);

            s.bbWidth = bbW; s.bbHeight = bbH;
            s.gravity = grav; s.friction = frict;
            s.lifetime = life; s.speedUpWhenYMotionIsBlocked = (su == 1);
            s.setPos(x, y, z);
            s.xo = x; s.yo = y; s.zo = z;
            s.xd = xd; s.yd = yd; s.zd = zd;
            for (int t = 0; t < ticks; ++t) s.tick();

            int bad = compareState(s, p, 14, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "MOVE") {
            // MOVE x y z bbW bbH mx my mz | <21 state>
            if (p.size() < 9 + 21) continue;
            double x = bd(p[1]), y = bd(p[2]), z = bd(p[3]);
            float bbW = bf(p[4]), bbH = bf(p[5]);
            double mx = bd(p[6]), my = bd(p[7]), mz = bd(p[8]);
            s.bbWidth = bbW; s.bbHeight = bbH;
            s.setPos(x, y, z);
            s.move(mx, my, mz);
            int bad = compareState(s, p, 9, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "SIZE") {
            // SIZE x y z bbW bbH nw nh | <21 state>
            if (p.size() < 8 + 21) continue;
            double x = bd(p[1]), y = bd(p[2]), z = bd(p[3]);
            float bbW = bf(p[4]), bbH = bf(p[5]);
            float nw = bf(p[6]), nh = bf(p[7]);
            s.bbWidth = bbW; s.bbHeight = bbH;
            s.setPos(x, y, z);
            s.setSize(nw, nh);
            int bad = compareState(s, p, 8, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "POS") {
            // POS bbW bbH x y z | <21 state>
            if (p.size() < 6 + 21) continue;
            float bbW = bf(p[1]), bbH = bf(p[2]);
            double x = bd(p[3]), y = bd(p[4]), z = bd(p[5]);
            s.bbWidth = bbW; s.bbHeight = bbH;
            s.setPos(x, y, z);
            int bad = compareState(s, p, 6, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "POWER") {
            // POWER xd yd zd power | <21 state>
            if (p.size() < 5 + 21) continue;
            s.xd = bd(p[1]); s.yd = bd(p[2]); s.zd = bd(p[3]);
            float power = bf(p[4]);
            s.setPower(power);
            int bad = compareState(s, p, 5, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        } else if (tag == "SCALE") {
            // SCALE x y z bbW bbH s | <21 state>
            if (p.size() < 7 + 21) continue;
            double x = bd(p[1]), y = bd(p[2]), z = bd(p[3]);
            float bbW = bf(p[4]), bbH = bf(p[5]);
            float sc = bf(p[6]);
            s.bbWidth = bbW; s.bbHeight = bbH;
            s.setPos(x, y, z);
            s.scale(sc);
            int bad = compareState(s, p, 7, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        }
        // unknown tags ignored
    }

    std::cout << "ParticlePhysics checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
