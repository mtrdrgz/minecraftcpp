// Parity gate for the C++ Beardifier port vs the REAL
// net.minecraft.world.level.levelgen.Beardifier. Ground truth:
// tools/BeardifierParity.java (drives the real class' @VisibleForTesting ctor +
// compute over random pieces/junctions/positions). This reproduces compute() from
// the same cases and compares the raw double bits.
//
//   beardifier_parity --cases build/beardifier.tsv

#include "Beardifier.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> splitTab(const std::string& s) {
    std::vector<std::string> o;
    std::string c;
    std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    if (!o.empty() && !o.back().empty() && o.back().back() == '\r') o.back().pop_back();
    return o;
}
std::int64_t rawBits(double d) {
    std::int64_t b;
    std::memcpy(&b, &d, sizeof(b));
    return b;
}
}  // namespace

int main(int argc, char** argv) {
    using namespace mc::levelgen;
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: beardifier_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    Beardifier current;
    long checks = 0, mismatches = 0, cases = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto f = splitTab(line);
        if (f[0] == "CASE") {
            ++cases;
            std::size_t i = 1;
            // NOTE: hoist every read into a sequenced local — C++ does NOT order
            // function-argument evaluation, so passing several next() calls into one
            // constructor would scramble the fields (AGENTS.md RULE: never put
            // multiple side-effecting reads in one argument list).
            auto next = [&] { return std::stoi(f[i++]); };
            int aMinX = next(), aMinY = next(), aMinZ = next(), aMaxX = next(), aMaxY = next(), aMaxZ = next();
            structure::BoundingBox affected(aMinX, aMinY, aMinZ, aMaxX, aMaxY, aMaxZ);
            int nP = next();
            std::vector<Beardifier::Rigid> pieces;
            for (int p = 0; p < nP; ++p) {
                int x0 = next(), y0 = next(), z0 = next(), x1 = next(), y1 = next(), z1 = next();
                Beardifier::Rigid r;
                r.box = structure::BoundingBox(x0, y0, z0, x1, y1, z1);
                r.terrainAdjustment = static_cast<TerrainAdjustment>(next());
                r.groundLevelDelta = next();
                pieces.push_back(r);
            }
            int nJ = next();
            std::vector<Beardifier::Junction> junctions;
            for (int j = 0; j < nJ; ++j) {
                Beardifier::Junction jc;
                jc.sourceX = next();
                jc.sourceGroundY = next();
                jc.sourceZ = next();
                junctions.push_back(jc);
            }
            current = Beardifier(std::move(pieces), std::move(junctions), affected);
        } else if (f[0] == "S") {
            int x = std::stoi(f[1]), y = std::stoi(f[2]), z = std::stoi(f[3]);
            std::int64_t expected = std::stoll(f[4]);
            std::int64_t got = rawBits(current.compute(x, y, z));
            ++checks;
            if (got != expected) {
                if (mismatches < 10)
                    std::cerr << "MISMATCH @(" << x << "," << y << "," << z << ") got=" << got
                              << " exp=" << expected << "\n";
                ++mismatches;
            }
        }
    }

    std::cout << "Beardifier cases=" << cases << " checks=" << checks
              << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
