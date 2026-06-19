// Bit-exact parity gate for the RNG-driven StrongholdPieces small-door selector,
// ported in
//   world/level/levelgen/structure/structures/StrongholdSmallDoor.h
// which reuses the certified RandomSource.h/.cpp (seeded LegacyRandomSource via
// RandomSource::create).
//
// Ground truth (mcpp/tools/StrongholdSmallDoorParity.java) drives the REAL
//   net.minecraft...structures.StrongholdPieces$StrongholdPiece
//       .randomSmallDoor(RandomSource)
// reflectively on an Unsafe-allocated concrete piece, seeded with
// RandomSource.create(seed). It records, per case: COUNT consecutive
// SmallDoorType ordinals, then one witness nextLong().
//
// We replay the same seeded LegacyRandomSource here, call our randomSmallDoor()
// COUNT times, then draw the witness nextLong(), and compare bit-for-bit. The
// witness proves the port consumes exactly ONE nextInt(5) draw per call (i.e.
// leaves the RNG stream in the same state Java does).
//
//   stronghold_small_door_parity --cases mcpp/build/stronghold_small_door.tsv

#include "world/level/levelgen/structure/structures/StrongholdSmallDoor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace ss = mc::levelgen::structure::structures;

// INT_MIN/INT_MAX- and LONG-safe decimal parse.
static int64_t i64(const std::string& s) { return std::stoll(s); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream sstr(line);
    while (std::getline(sstr, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: stronghold_small_door_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> f = split(line);
        // SSD  seed  count  ord0..ord{count-1}  afterLong
        if (f.empty() || f[0] != "SSD") continue;
        if (f.size() < 3) {
            fail(line);
            continue;
        }
        const int64_t seed = i64(f[1]);
        const int count = static_cast<int>(i64(f[2]));
        // Expected layout: 3 fixed cols + count ordinals + 1 witness long.
        if (count < 0 || f.size() != static_cast<size_t>(3 + count + 1)) {
            fail(line);
            continue;
        }

        mc::levelgen::LegacyRandomSource random(seed);

        bool ok = true;
        for (int k = 0; k < count; ++k) {
            ss::SmallDoorType door = ss::randomSmallDoor(random);
            const int64_t got = static_cast<int32_t>(door);
            const int64_t want = i64(f[3 + k]);
            if (got != want) {
                ok = false;
                break;
            }
        }

        if (ok) {
            const int64_t gotLong = random.nextLong();
            const int64_t wantLong = i64(f[3 + count]);
            if (gotLong != wantLong) ok = false;
        }

        ++checks;
        if (!ok) fail(line);
    }

    std::cout << "StrongholdSmallDoor checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
