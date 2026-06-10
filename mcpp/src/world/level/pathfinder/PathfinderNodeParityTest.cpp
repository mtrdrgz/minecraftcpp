// Bit-exact parity gate for net.minecraft.world.level.pathfinder.Node pure helpers.
// Reads the TSV emitted by PathfinderNodeParity.java, recomputes via the C++ port,
// and compares BIT-FOR-BIT (std::bit_cast), never by value.

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "world/level/pathfinder/Node.h"

using mc::pathfinder::Node;
using mc::BlockPos;

// ── bit-exact exchange helpers ────────────────────────────────────────────────
static float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<std::uint32_t>(std::stoul(s, nullptr, 16))); }
static std::uint32_t fb(float v)          { return std::bit_cast<std::uint32_t>(v); }

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string tsv;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) tsv = argv[++i];
    }
    if (tsv.empty()) { std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]); return 2; }

    std::ifstream in(tsv);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", tsv.c_str()); return 2; }

    long cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        auto t = split(line);
        if (t.empty()) continue;
        const std::string& tag = t[0];

        if (tag == "HASH") {
            // HASH  x  y  z  hash(int decimal)
            if (t.size() < 5) continue;
            int x = std::stoi(t[1]), y = std::stoi(t[2]), z = std::stoi(t[3]);
            std::int32_t exp = static_cast<std::int32_t>(std::stoll(t[4]));
            std::int32_t got = Node::createHash(x, y, z);
            ++cases;
            if (got != exp) {
                ++mismatches;
                if (mismatches <= 20)
                    std::fprintf(stderr, "HASH mismatch x=%d y=%d z=%d exp=%d got=%d\n", x, y, z, exp, got);
            }
            continue;
        }

        // All distance tags: <TAG> ax ay az bx by bz <floatbits>
        if (t.size() < 8) continue;
        int ax = std::stoi(t[1]), ay = std::stoi(t[2]), az = std::stoi(t[3]);
        int bx = std::stoi(t[4]), by = std::stoi(t[5]), bz = std::stoi(t[6]);
        float exp = bf(t[7]);

        Node a(ax, ay, az);
        Node b(bx, by, bz);
        BlockPos bp{bx, by, bz};
        float got = 0.0f;
        bool known = true;

        if      (tag == "DIST")     got = a.distanceTo(b);
        else if (tag == "DISTB")    got = a.distanceTo(bp);
        else if (tag == "DISTXZ")   got = a.distanceToXZ(b);
        else if (tag == "DISTSQR")  got = a.distanceToSqr(b);
        else if (tag == "DISTSQRB") got = a.distanceToSqr(bp);
        else if (tag == "MAN")      got = a.distanceManhattan(b);
        else if (tag == "MANB")     got = a.distanceManhattan(bp);
        else known = false;

        if (!known) continue;
        ++cases;
        if (fb(got) != fb(exp)) {
            ++mismatches;
            if (mismatches <= 20)
                std::fprintf(stderr, "%s mismatch a=(%d,%d,%d) b=(%d,%d,%d) exp=%08x got=%08x\n",
                             tag.c_str(), ax, ay, az, bx, by, bz, fb(exp), fb(got));
        }
    }

    std::printf("PathfinderNode cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
