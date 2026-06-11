// Parity test for mcpp/src/client/particle/FlyTowardsPositionParticle.h — the pure
// per-tick motion update of net.minecraft.client.particle.FlyTowardsPositionParticle
// (Minecraft 26.1.2).
//
// Ground truth: tools/FlyTowardsPositionParticleParity.java, which allocateInstance()s a
// REAL FlyTowardsPositionParticle (no constructor -> no RandomSource, no ClientLevel),
// reflectively seeds its position state, and drives its real overriding tick() body,
// emitting the resulting position state. Each row carries the inputs + 8 state columns;
// here we replay the same op sequence through FlyTowardsPositionParticle and compare
// BIT-FOR-BIT.
//
//   fly_towards_position_parity --cases mcpp/build/fly_towards_position.tsv

#include "FlyTowardsPositionParticle.h"

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
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

// Compare the live C++ state against the 8 expected state columns beginning at index `base`.
// Returns the number of mismatched fields (0 == bit-exact); prints a diagnostic on failure.
int compareState(const cp::FlyTowardsPositionParticle& s, const std::vector<std::string>& p,
                 std::size_t base, const std::string& tag, long long mismatches) {
    struct DChk { const char* name; uint64_t got; };
    struct IChk { const char* name; int got; };

    const DChk dchecks[] = {
        {"xo", db(s.xo)}, {"yo", db(s.yo)}, {"zo", db(s.zo)},
        {"x", db(s.x)},   {"y", db(s.y)},   {"z", db(s.z)},
    };
    const IChk ichecks[] = {
        {"age", s.age}, {"removed", s.removed ? 1 : 0},
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
    if (casesPath.empty()) { std::cerr << "usage: fly_towards_position_parity --cases <tsv>\n"; return 2; }

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
            // TICK ox oy oz xd yd zd xStart yStart zStart life ticks | <8 state>
            if (p.size() < 12 + 8) continue;
            // ox/oy/oz (cols 1..3) duplicate xStart/yStart/zStart (cols 7..9); use the
            // xStart columns, which are what tick() actually reads.
            double xd = bd(p[4]), yd = bd(p[5]), zd = bd(p[6]);
            double xStart = bd(p[7]), yStart = bd(p[8]), zStart = bd(p[9]);
            int life = std::stoi(p[10]);
            int ticks = std::stoi(p[11]);

            cp::FlyTowardsPositionParticle s;
            s.xStart = xStart; s.yStart = yStart; s.zStart = zStart;
            s.xd = xd; s.yd = yd; s.zd = zd;
            s.lifetime = life;
            // mirror the constructor: xo = x = xStart + xd, etc. (spawn at the target).
            double sx = xStart + xd, sy = yStart + yd, sz = zStart + zd;
            s.x = sx;  s.y = sy;  s.z = sz;
            s.xo = sx; s.yo = sy; s.zo = sz;
            for (int t = 0; t < ticks; ++t) s.tick();

            int bad = compareState(s, p, 12, tag, mismatches);
            ++checks;
            if (bad) ++mismatches;
        }
        // unknown tags ignored
    }

    std::cout << "FlyTowardsPositionParticle checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
