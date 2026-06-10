// Bit-exact parity gate for net.minecraft.world.level.pathfinder.PathType.
// Reads the TSV emitted by PathTypeParity.java and verifies, for every enum
// constant, that the C++ port agrees on:
//   * values().length  (COUNT row)
//   * ordinal()        (decimal)
//   * name()           (raw string)
//   * getMalus()       (compared BIT-FOR-BIT via std::bit_cast, never by value)

#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "world/level/pathfinder/PathType.h"

using mc::pathfinder::PathType;
using mc::pathfinder::PATH_TYPE_COUNT;

static float bf(const std::string& s) {
    return std::bit_cast<float>(static_cast<std::uint32_t>(std::stoul(s, nullptr, 16)));
}
static std::uint32_t fb(float v) { return std::bit_cast<std::uint32_t>(v); }

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
        if (line.back() == '\r') line.pop_back();
        auto t = split(line);
        if (t.empty()) continue;
        const std::string& tag = t[0];

        if (tag == "COUNT") {
            // COUNT  <values().length>
            if (t.size() < 2) continue;
            long exp = std::stol(t[1]);
            ++cases;
            if (exp != static_cast<long>(PATH_TYPE_COUNT)) {
                ++mismatches;
                std::fprintf(stderr, "COUNT mismatch exp=%ld got=%zu\n", exp, PATH_TYPE_COUNT);
            }
            continue;
        }

        if (tag == "PT") {
            // PT  ordinal  name  malusBits
            if (t.size() < 4) continue;
            int ordinal = std::stoi(t[1]);
            const std::string& name = t[2];
            float malusExp = bf(t[3]);

            ++cases;

            if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= PATH_TYPE_COUNT) {
                ++mismatches;
                std::fprintf(stderr, "PT ordinal out of range: %d (name=%s)\n", ordinal, name.c_str());
                continue;
            }

            PathType pt = static_cast<PathType>(ordinal);

            // ordinal round-trip.
            if (mc::pathfinder::ordinal(pt) != ordinal) {
                ++mismatches;
                std::fprintf(stderr, "ordinal mismatch exp=%d got=%d\n", ordinal, mc::pathfinder::ordinal(pt));
            }

            // name().
            std::string_view gotName = mc::pathfinder::name(pt);
            if (gotName != name) {
                ++mismatches;
                std::fprintf(stderr, "name mismatch ord=%d exp=%s got=%.*s\n",
                             ordinal, name.c_str(), static_cast<int>(gotName.size()), gotName.data());
            }

            // getMalus() — bit-for-bit.
            float malusGot = mc::pathfinder::getMalus(pt);
            if (fb(malusGot) != fb(malusExp)) {
                ++mismatches;
                std::fprintf(stderr, "getMalus mismatch %s(ord=%d) exp=%08x got=%08x\n",
                             name.c_str(), ordinal, fb(malusExp), fb(malusGot));
            }
            continue;
        }
    }

    std::printf("PathType cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
