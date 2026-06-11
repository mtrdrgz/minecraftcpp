// Parity test for mcpp/src/client/particle/PortalParticle.h — the pure per-tick
// motion update + quad-size easing of net.minecraft.client.particle.PortalParticle
// (Minecraft 26.1.2).
//
// Ground truth: tools/PortalParticleParity.java, which allocateInstance()s a REAL
// PortalParticle (no constructor -> no RandomSource, no ClientLevel), reflectively
// seeds its position state, and drives its real overriding tick() body and
// getQuadSize() override, emitting the resulting position state + quad-size. Each
// row carries the inputs + 8 state columns + the getQuadSize result; here we
// replay the same op sequence through PortalParticle and compare BIT-FOR-BIT.
//
//   portal_particle_parity --cases mcpp/build/portal_particle.tsv

#include "PortalParticle.h"

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

// Compare the live C++ state against the 8 expected state columns beginning at index `base`.
// Returns the number of mismatched fields (0 == bit-exact); prints a diagnostic on failure.
int compareState(const cp::PortalParticle& s, const std::vector<std::string>& p,
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
    if (casesPath.empty()) { std::cerr << "usage: portal_particle_parity --cases <tsv>\n"; return 2; }

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
            // TICK ox oy oz xd yd zd life ticks aBits qsInBits | <8 state> qsOutBits
            if (p.size() < 11 + 8 + 1) continue;
            double xStart = bd(p[1]), yStart = bd(p[2]), zStart = bd(p[3]);
            double xd = bd(p[4]), yd = bd(p[5]), zd = bd(p[6]);
            int life = std::stoi(p[7]);
            int ticks = std::stoi(p[8]);
            float aVal = bf(p[9]);
            float qsIn = bf(p[10]);

            cp::PortalParticle s;
            s.xStart = xStart; s.yStart = yStart; s.zStart = zStart;
            s.xd = xd; s.yd = yd; s.zd = zd;
            s.lifetime = life;
            s.quadSize = qsIn;
            // mirror the constructor: xo = x = xStart (spawn at origin).
            s.x = xStart;  s.y = yStart;  s.z = zStart;
            s.xo = xStart; s.yo = yStart; s.zo = zStart;
            for (int t = 0; t < ticks; ++t) s.tick();

            int bad = compareState(s, p, 11, tag, mismatches);

            // getQuadSize(aVal) result is the last column (index 11 + 8 = 19).
            float qsOut = s.getQuadSize(aVal);
            uint32_t expectQs = static_cast<uint32_t>(std::stoul(p[19], nullptr, 16));
            if (fb(qsOut) != expectQs) {
                ++bad;
                if (mismatches + bad <= 20)
                    std::cerr << tag << " field=getQuadSize expect=" << std::hex << expectQs
                              << " got=" << fb(qsOut) << std::dec << "\n";
            }

            ++checks;
            if (bad) ++mismatches;
        }
        // unknown tags ignored
    }

    std::cout << "PortalParticle checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
