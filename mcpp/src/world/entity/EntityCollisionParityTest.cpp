// Parity test for Entity.collideWithShapes. Ground truth: tools/EntityCollisionParity.java.
// Builds the same single-box VoxelShapes via Shapes::create and runs the certified
// collide slide; bit-exact (doubles as raw IEEE-754 bits).
//
//   entity_collision_parity --cases mcpp/build/entity_collision.tsv

#include "EntityCollision.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Vec3;
using mc::AABB;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: entity_collision_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p[0] != "COLLIDE") { ++mism; if (shown++ < 30) std::cerr << "UNKNOWN_TAG " << p[0] << "\n"; continue; }
        ++total;
        int i = 1;
        AABB box(bd(p[i]), bd(p[i+1]), bd(p[i+2]), bd(p[i+3]), bd(p[i+4]), bd(p[i+5])); i += 6;
        Vec3 movement{bd(p[i]), bd(p[i+1]), bd(p[i+2])}; i += 3;
        int nshapes = std::stoi(p[i]); ++i;
        std::vector<mc::VoxelShapePtr> shapes;
        for (int s = 0; s < nshapes; ++s) {
            shapes.push_back(mc::Shapes::create(AABB(bd(p[i]), bd(p[i+1]), bd(p[i+2]), bd(p[i+3]), bd(p[i+4]), bd(p[i+5]))));
            i += 6;
        }
        Vec3 r = mc::collideWithShapes(movement, box, shapes);
        if (db(r.x) != std::stoull(p[i], nullptr, 16) || db(r.y) != std::stoull(p[i+1], nullptr, 16) || db(r.z) != std::stoull(p[i+2], nullptr, 16)) {
            ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << line << "\n";
        }
    }
    std::cout << "EntityCollision cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
