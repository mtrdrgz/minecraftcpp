// Bit-exact parity gate for the star-field geometry produced by
// net.minecraft.client.renderer.SkyRenderer.buildStars() (Minecraft 26.1.2).
//
// Reads the TSV emitted by tools/StarGeometryParity.java (which re-drives the
// real RandomSource/Mth/JOML classes) and re-drives the C++ port in
// render/StarGeometry.h, comparing every quad-corner coordinate bit-for-bit
// (Float.floatToRawIntBits on the Java side vs std::bit_cast<int32_t> here).
//
//   mcpp/build/star_geometry_parity.exe --cases mcpp/build/star_geometry.tsv
//
// TSV rows:
//   STAR  <i>  <x0> <y0> <z0> <x1> <y1> <z1> <x2> <y2> <z2> <x3> <y3> <z3>
//     where <i> is the accepting loop index and the 12 values are raw int bits.
//   COUNT <n>  total accepted stars (cross-checks the C++ accept count).

#include "render/StarGeometry.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

int32_t f2i(float f) { return std::bit_cast<int32_t>(f); }

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: star_geometry_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // Re-drive the C++ port once; index the emitted vertices in groups of 4.
    const std::vector<mc::render::star::StarVertex> verts = mc::render::star::buildStars();
    if (verts.size() % 4 != 0) {
        std::cerr << "C++ emitted " << verts.size() << " verts (not a multiple of 4)\n";
        return 2;
    }
    const long cppStars = static_cast<long>(verts.size() / 4);

    long checks = 0, mism = 0;
    long gtRows = 0;          // STAR rows seen
    long gtCount = -1;        // COUNT row value
    long starOrdinal = 0;     // which accepted star (0-based) this STAR row is

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;

        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];
        if (tag == "COUNT") {
            if (p.size() >= 2) gtCount = std::stol(p[1]);
            continue;
        }
        if (tag != "STAR") continue;  // forward-compatible: ignore unknown tags
        if (p.size() != 14) {
            std::cerr << "MALFORMED STAR row cols=" << p.size() << "\n";
            ++mism;
            continue;
        }
        ++gtRows;

        // The C++ accepts the same stars in the same order, so the N-th STAR row
        // corresponds to C++ vertices [4*starOrdinal .. 4*starOrdinal+3].
        if (starOrdinal >= cppStars) {
            std::cerr << "GT has more accepted stars than C++ (extra at GT i=" << p[1] << ")\n";
            ++mism;
            ++starOrdinal;
            continue;
        }

        // 12 expected int bits, in v0(x,y,z) v1 v2 v3 order.
        for (int v = 0; v < 4; ++v) {
            const mc::render::star::StarVertex& sv = verts[4 * starOrdinal + v];
            const int32_t gotBits[3] = {f2i(sv.x), f2i(sv.y), f2i(sv.z)};
            for (int c = 0; c < 3; ++c) {
                ++checks;
                int32_t expected =
                    static_cast<int32_t>(std::stoll(p[2 + 3 * v + c]));
                if (gotBits[c] != expected) {
                    ++mism;
                    if (mism <= 20) {
                        std::cerr << "MISMATCH star_i=" << p[1] << " vertex=" << v
                                  << " comp=" << c << " exp=" << expected
                                  << " got=" << gotBits[c] << "\n";
                    }
                }
            }
        }
        ++starOrdinal;
    }

    // Cross-check accepted-star counts (GT STAR rows, GT COUNT, C++ stars).
    if (gtCount >= 0 && gtCount != gtRows) {
        std::cerr << "GT COUNT=" << gtCount << " != STAR rows=" << gtRows << "\n";
        ++mism;
    }
    if (gtRows != cppStars) {
        std::cerr << "accepted-star count mismatch: GT=" << gtRows
                  << " C++=" << cppStars << "\n";
        ++mism;
    }

    std::cout << "StarGeometry checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
