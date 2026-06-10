// ExplosionMathParityTest — bit-for-bit gate for ExplosionMath.h vs the real
// net.minecraft explosion falloff math (ground truth: tools/ExplosionMathParity.java).
//
// TAGs (tab-separated rows; floats=%08x raw bits, doubles=%016x raw bits):
//   DIST     radius  distSq                            -> distRatio(double)
//   DMG      radius  distSq  exposure                  -> getEntityDamageAmount(float)
//   DMG_REAL radius  distSq  exposure                  -> real ExplosionDamageCalculator
//                                                          .getEntityDamageAmount(float)
//                                                          (recomputed by the SAME C++
//                                                          path — confirms real==inlined)
//   KB       radius  distSq  exposure  kbMul  kbRes     -> getKnockbackPower(double)
//
// Every comparison is std::bit_cast (never by value).
//   explosion_falloff_parity --cases explosion_falloff.tsv

#include "ExplosionMath.h"

#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
static float  bf(const std::string& s) { return std::bit_cast<float>((uint32_t)std::stoul(s, nullptr, 16)); }
static uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
static uint32_t fb(float v)  { return std::bit_cast<uint32_t>(v); }

int main(int argc, char** argv) {
    const char* casesPath = nullptr;
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (!casesPath) { std::fprintf(stderr, "usage: explosion_falloff_parity --cases <tsv>\n"); return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", casesPath); return 2; }

    long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::vector<std::string> f;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) f.push_back(tok);
        if (f.empty()) continue;
        const std::string& tag = f[0];

        if (tag == "DIST") {
            // DIST radius distSq -> distRatio
            float radius = bf(f[1]);
            double dsq = bd(f[2]);
            uint64_t want = std::stoull(f[3], nullptr, 16);
            uint64_t got = db(mc::explosion::distanceRatio(radius, dsq));
            cases++;
            if (got != want) {
                mismatches++;
                std::fprintf(stderr, "DIST mismatch r=%08x dsq=%016llx want=%016llx got=%016llx\n",
                             fb(radius), (unsigned long long)std::stoull(f[2], nullptr, 16),
                             (unsigned long long)want, (unsigned long long)got);
            }
        } else if (tag == "DMG" || tag == "DMG_REAL") {
            // DMG radius distSq exposure -> getEntityDamageAmount
            float radius = bf(f[1]);
            double dsq = bd(f[2]);
            float exposure = bf(f[3]);
            uint32_t want = (uint32_t)std::stoul(f[4], nullptr, 16);
            uint32_t got = fb(mc::explosion::getEntityDamageAmount(radius, dsq, exposure));
            cases++;
            if (got != want) {
                mismatches++;
                std::fprintf(stderr, "%s mismatch r=%08x dsq=%016llx exp=%08x want=%08x got=%08x\n",
                             tag.c_str(), fb(radius),
                             (unsigned long long)std::stoull(f[2], nullptr, 16),
                             fb(exposure), want, got);
            }
        } else if (tag == "KB") {
            // KB radius distSq exposure kbMul kbRes -> getKnockbackPower
            float radius = bf(f[1]);
            double dsq = bd(f[2]);
            float exposure = bf(f[3]);
            float kbMul = bf(f[4]);
            double kbRes = bd(f[5]);
            uint64_t want = std::stoull(f[6], nullptr, 16);
            uint64_t got = db(mc::explosion::getKnockbackPower(radius, dsq, exposure, kbMul, kbRes));
            cases++;
            if (got != want) {
                mismatches++;
                std::fprintf(stderr, "KB mismatch r=%08x dsq=%016llx exp=%08x km=%08x kr=%016llx want=%016llx got=%016llx\n",
                             fb(radius),
                             (unsigned long long)std::stoull(f[2], nullptr, 16),
                             fb(exposure), fb(kbMul),
                             (unsigned long long)std::stoull(f[5], nullptr, 16),
                             (unsigned long long)want, (unsigned long long)got);
            }
        }
        // unknown tags ignored
    }

    std::printf("ExplosionMath cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
