// Parity test for the pure integer state machine of (Minecraft 26.1.2):
//   net.minecraft.world.entity.monster.warden.WardenSpawnTracker
//
// Ground truth: tools/WardenSpawnTrackerParity.java constructs a REAL instance
// (public (int,int,int) ctor — no Level/registry), drives the REAL pure methods
// via reflection, and emits the full post-state per op -> TSV. This test re-seeds
// the C++ port (WardenSpawnTracker.h) with the same { ticks, level, cooldown },
// applies the same operation, and compares all three result ints exactly.
//
// All outputs are plain 32-bit ints, so comparison is decimal/exact (no float bit
// exchange needed). Mismatch == a real port bug (wrong threshold edge, missing
// cooldown gate, wrong clamp band) — fix the port, never the test.
//
//   warden_spawn_tracker_parity --cases mcpp/build/warden_spawn_tracker.tsv

#include "world/entity/monster/warden/WardenSpawnTracker.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// TSV ints are plain decimal (may be INT_MIN/INT_MAX). Parse as long long then
// narrow to int — inputs are valid 32-bit ints by construction.
int i(const std::string& s) { return static_cast<int>(std::stoll(s)); }

} // namespace

int main(int argc, char** argv) {
    using mc::world::entity::monster::warden::WardenSpawnTracker;

    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: warden_spawn_tracker_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, const std::string& got) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "  got=" << got << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "STEP") {
            // STEP  seed_t seed_w seed_c  opcode oparg  out_t out_w out_c
            int st = i(p[1]), sw = i(p[2]), sc = i(p[3]);
            int opcode = i(p[4]), oparg = i(p[5]);
            int et = i(p[6]), ew = i(p[7]), ec = i(p[8]);

            WardenSpawnTracker w(st, sw, sc);
            switch (opcode) {
                case 0: w.tick(); break;
                case 1: w.setWarningLevel(oparg); break;
                case 2: w.increaseWarningLevel(); break;
                case 3: w.decreaseWarningLevel(); break;
                case 4: w.reset(); break;
                default: fail(line, "BAD_OPCODE"); continue;
            }
            if (w.ticksSinceLastWarning != et || w.warningLevel != ew || w.cooldownTicks != ec) {
                fail(line, std::to_string(w.ticksSinceLastWarning) + "," +
                            std::to_string(w.warningLevel) + "," +
                            std::to_string(w.cooldownTicks));
            }
        } else if (tag == "TICKN") {
            // TICKN seed_t seed_w seed_c n  out_t out_w out_c
            int st = i(p[1]), sw = i(p[2]), sc = i(p[3]);
            int n = i(p[4]);
            int et = i(p[5]), ew = i(p[6]), ec = i(p[7]);

            WardenSpawnTracker w(st, sw, sc);
            for (int k = 0; k < n; ++k) w.tick();
            if (w.ticksSinceLastWarning != et || w.warningLevel != ew || w.cooldownTicks != ec) {
                fail(line, std::to_string(w.ticksSinceLastWarning) + "," +
                            std::to_string(w.warningLevel) + "," +
                            std::to_string(w.cooldownTicks));
            }
        } else {
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "WardenSpawnTracker checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
