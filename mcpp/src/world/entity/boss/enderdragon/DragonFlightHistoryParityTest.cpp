// Parity test for mcpp/src/world/entity/boss/enderdragon/DragonFlightHistory.h —
// the ender-dragon body-trail ring buffer of
// net.minecraft.world.entity.boss.enderdragon.DragonFlightHistory (Minecraft 26.1.2).
//
// Ground truth: tools/DragonFlightHistoryParity.java vs the REAL net.minecraft class.
// The C++ side reconstructs the identical history (same seed + record count via the
// same yFor/yRotFor sequence), then drives the C++ get(delay) / get(delay, partial) /
// copyFrom and compares the resulting Sample (y, yRot) and head index BIT-FOR-BIT
// against the values the real Java class produced.
//
//   dragon_flight_history_parity --cases mcpp/build/dragon_flight_history.tsv

#include "DragonFlightHistory.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace dfh = mc::world::entity::boss::enderdragon;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<std::uint32_t>(std::stoul(s, nullptr, 16))); }
double   bd(const std::string& s) { return std::bit_cast<double>(static_cast<std::uint64_t>(std::stoull(s, nullptr, 16))); }
std::uint32_t fb(float v)  { return std::bit_cast<std::uint32_t>(v); }
std::uint64_t db(double v) { return std::bit_cast<std::uint64_t>(v); }

// MUST match the deterministic generators in DragonFlightHistoryParity.java exactly.
// Pure rational arithmetic only (NO std::sin / libm) so the rebuilt input history is
// bit-identical on both sides; the per-record y/yRot are inputs to the certified ring
// buffer, not the thing under test.
double yFor(int seed, int i) {
    return (seed * 13 + i * 7) * 0.5 - 96.0 + ((i * 73 % 17) - 8) * 0.625;
}
float yRotFor(int seed, int i) {
    return static_cast<float>((seed * 37 + i * 53) % 720) - 360.0F + (i % 5) * 11.25F;
}

dfh::DragonFlightHistory build(int seed, int nRecords) {
    dfh::DragonFlightHistory h;
    for (int i = 0; i < nRecords; ++i) h.record(yFor(seed, i), yRotFor(seed, i));
    return h;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: dragon_flight_history_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long checks = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "RECORD") {
            // RECORD seed nRecords delay | yBits(d) yRotBits(f) head
            if (p.size() < 7) continue;
            int seed = std::stoi(p[1]);
            int nr = std::stoi(p[2]);
            int delay = std::stoi(p[3]);
            std::uint64_t eY = static_cast<std::uint64_t>(std::stoull(p[4], nullptr, 16));
            std::uint32_t eYRot = static_cast<std::uint32_t>(std::stoul(p[5], nullptr, 16));
            int eHead = std::stoi(p[6]);

            dfh::DragonFlightHistory h = build(seed, nr);
            dfh::DragonFlightHistory::Sample s = h.get(delay);
            ++checks;
            bool bad = db(s.y) != eY || fb(s.yRot) != eYRot || h.head() != eHead;
            if (bad) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "RECORD seed=" << seed << " nr=" << nr << " delay=" << delay
                              << " yE=" << p[4] << " yG=" << std::hex << db(s.y)
                              << " yRotE=" << p[5] << " yRotG=" << fb(s.yRot) << std::dec
                              << " headE=" << eHead << " headG=" << h.head() << "\n";
            }
        } else if (tag == "INTERP") {
            // INTERP seed nRecords delay partialBits(f) | yBits(d) yRotBits(f)
            if (p.size() < 7) continue;
            int seed = std::stoi(p[1]);
            int nr = std::stoi(p[2]);
            int delay = std::stoi(p[3]);
            float partial = bf(p[4]);
            std::uint64_t eY = static_cast<std::uint64_t>(std::stoull(p[5], nullptr, 16));
            std::uint32_t eYRot = static_cast<std::uint32_t>(std::stoul(p[6], nullptr, 16));

            dfh::DragonFlightHistory h = build(seed, nr);
            dfh::DragonFlightHistory::Sample s = h.get(delay, partial);
            ++checks;
            bool bad = db(s.y) != eY || fb(s.yRot) != eYRot;
            if (bad) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "INTERP seed=" << seed << " nr=" << nr << " delay=" << delay
                              << " p=" << p[4]
                              << " yE=" << p[5] << " yG=" << std::hex << db(s.y)
                              << " yRotE=" << p[6] << " yRotG=" << fb(s.yRot) << std::dec << "\n";
            }
        } else if (tag == "COPY") {
            // COPY seed nRecords delay | yBits(d) yRotBits(f) head srcHead
            if (p.size() < 8) continue;
            int seed = std::stoi(p[1]);
            int nr = std::stoi(p[2]);
            int delay = std::stoi(p[3]);
            std::uint64_t eY = static_cast<std::uint64_t>(std::stoull(p[4], nullptr, 16));
            std::uint32_t eYRot = static_cast<std::uint32_t>(std::stoul(p[5], nullptr, 16));
            int eHead = std::stoi(p[6]);
            int eSrcHead = std::stoi(p[7]);

            dfh::DragonFlightHistory src = build(seed, nr);
            dfh::DragonFlightHistory dst;
            dst.copyFrom(src);
            dfh::DragonFlightHistory::Sample s = dst.get(delay);
            ++checks;
            bool bad = db(s.y) != eY || fb(s.yRot) != eYRot
                       || dst.head() != eHead || src.head() != eSrcHead;
            if (bad) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "COPY seed=" << seed << " nr=" << nr << " delay=" << delay
                              << " yE=" << p[4] << " yG=" << std::hex << db(s.y)
                              << " yRotE=" << p[5] << " yRotG=" << fb(s.yRot) << std::dec
                              << " headE=" << eHead << " headG=" << dst.head()
                              << " srcHeadE=" << eSrcHead << " srcHeadG=" << src.head() << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "DragonFlightHistory checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
