// Parity test for net.minecraft.world.entity.ai.util.RandomPos.
//
// VERIFIES the 1:1 C++ port:
//   mc::entity::ai::generateRandomDirection
//   mc::entity::ai::generateRandomDirectionWithinRadians   (world/entity/ai/util/RandomPos.h)
// against ground truth from the REAL decompiled class, emitted by
//   mcpp/tools/RandomPosParity.java  -> random_pos.tsv
//
//   random_pos_parity --cases mcpp/build/random_pos.tsv
//
// Each row seeds a fresh LegacyRandomSource with <seed> (identical to the GT tool)
// so the RNG stream advances bit-for-bit. Input doubles arrive as raw IEEE-754
// long bits (hex) and are bit_cast back — no decimal round-trip. Output BlockPos
// coordinates are compared in decimal; the radial helper's null result is encoded
// as present=0.
//
// Row formats (tab-separated):
//   DIR  <seed>  <hDist> <vDist>  <x> <y> <z>
//   RAD  <seed>  <minH#> <maxH#> <vDist> <flyH> <xDir#> <zDir#> <maxRad#>  <present> <x> <y> <z>

#include "RandomPos.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::LegacyRandomSource;
namespace ai = mc::entity::ai;

namespace {

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, delim)) out.push_back(it);
    return out;
}

double db(const std::string& hex) {
    return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(hex, nullptr, 16)));
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: random_pos_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto failI = [&](const std::string& what, long got, long exp, const std::string& l) {
        ++mism;
        if (shown++ < 40)
            std::cerr << "MISMATCH " << what << " got=" << got << " exp=" << exp << " | " << l << "\n";
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line, '\t');
        const std::string& tag = p[0];

        if (tag == "DIR") {
            // DIR  seed  hDist  vDist  x y z
            int64_t seed = static_cast<int64_t>(std::stoll(p[1]));
            int32_t hDist = std::stoi(p[2]);
            int32_t vDist = std::stoi(p[3]);
            int32_t ex = std::stoi(p[4]), ey = std::stoi(p[5]), ez = std::stoi(p[6]);

            LegacyRandomSource r(seed);
            ai::BlockPosResult got = ai::generateRandomDirection(r, hDist, vDist);
            ++total;
            if (got.x != ex) failI("DIR.x", got.x, ex, line);
            else if (got.y != ey) failI("DIR.y", got.y, ey, line);
            else if (got.z != ez) failI("DIR.z", got.z, ez, line);
        } else if (tag == "RAD") {
            // RAD seed minH# maxH# vDist flyH xDir# zDir# maxRad#  present x y z
            int64_t seed = static_cast<int64_t>(std::stoll(p[1]));
            double minH = db(p[2]);
            double maxH = db(p[3]);
            int32_t vDist = std::stoi(p[4]);
            int32_t flyH = std::stoi(p[5]);
            double xDir = db(p[6]);
            double zDir = db(p[7]);
            double maxRad = db(p[8]);
            int present = std::stoi(p[9]);
            int32_t ex = std::stoi(p[10]), ey = std::stoi(p[11]), ez = std::stoi(p[12]);

            LegacyRandomSource r(seed);
            auto got = ai::generateRandomDirectionWithinRadians(
                r, minH, maxH, vDist, flyH, xDir, zDir, maxRad);
            ++total;
            int gotPresent = got.has_value() ? 1 : 0;
            if (gotPresent != present) {
                failI("RAD.present", gotPresent, present, line);
            } else if (present == 1) {
                if (got->x != ex) failI("RAD.x", got->x, ex, line);
                else if (got->y != ey) failI("RAD.y", got->y, ey, line);
                else if (got->z != ez) failI("RAD.z", got->z, ez, line);
            }
        }
        // unknown tags ignored
    }

    std::cout << "RandomPos checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
