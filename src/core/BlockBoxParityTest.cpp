// Parity test for the pure-integer surface of net.minecraft.core.BlockBox
// (Minecraft 26.1.2). Ground truth: tools/BlockBoxParity.java vs the real class.
// All values are ints, compared exactly.
//
//   block_box_parity --cases mcpp/build/block_box.tsv

#include "core/BlockBox.h"

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

int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

mc::Direction dirFrom3d(int v) { return static_cast<mc::Direction>(v); } // DOWN=0..EAST=5

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_box_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    // Compare a BlockBox against expected min(ex,ey,ez)/max(emx,emy,emz) string fields.
    auto eqBox = [&](const mc::BlockBox& b,
                     const std::string& ex, const std::string& ey, const std::string& ez,
                     const std::string& emx, const std::string& emy, const std::string& emz,
                     const std::string& l) {
        if (b.min.x != i(ex) || b.min.y != i(ey) || b.min.z != i(ez)
            || b.max.x != i(emx) || b.max.y != i(emy) || b.max.z != i(emz)) {
            fail(l + " got min=" + std::to_string(b.min.x) + "," + std::to_string(b.min.y) + "," + std::to_string(b.min.z)
                   + " max=" + std::to_string(b.max.x) + "," + std::to_string(b.max.y) + "," + std::to_string(b.max.z));
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "OFP") {
            // OFP  x y z   minX minY minZ  maxX maxY maxZ  isBlock
            mc::Vec3i pos(i(p[1]), i(p[2]), i(p[3]));
            mc::BlockBox box = mc::BlockBox::of(pos);
            eqBox(box, p[4], p[5], p[6], p[7], p[8], p[9], line);
            eqI(box.isBlock() ? 1 : 0, p[10], line);
        }
        else if (t == "OFAB") {
            // OFAB  ax ay az  bx by bz   minX minY minZ  maxX maxY maxZ  isBlock  sizeX sizeY sizeZ
            mc::Vec3i a(i(p[1]), i(p[2]), i(p[3])), b(i(p[4]), i(p[5]), i(p[6]));
            mc::BlockBox box = mc::BlockBox::of(a, b);
            eqBox(box, p[7], p[8], p[9], p[10], p[11], p[12], line);
            eqI(box.isBlock() ? 1 : 0, p[13], line);
            eqI(box.sizeX(), p[14], line);
            eqI(box.sizeY(), p[15], line);
            eqI(box.sizeZ(), p[16], line);
        }
        else if (t == "INC") {
            // INC  minX minY minZ  maxX maxY maxZ  prX prY prZ   nMinX nMinY nMinZ  nMaxX nMaxY nMaxZ
            mc::BlockBox box(mc::Vec3i(i(p[1]), i(p[2]), i(p[3])), mc::Vec3i(i(p[4]), i(p[5]), i(p[6])));
            mc::BlockBox inc = box.include(mc::Vec3i(i(p[7]), i(p[8]), i(p[9])));
            eqBox(inc, p[10], p[11], p[12], p[13], p[14], p[15], line);
        }
        else if (t == "CON") {
            // CON  minX minY minZ  maxX maxY maxZ  prX prY prZ  contains
            mc::BlockBox box(mc::Vec3i(i(p[1]), i(p[2]), i(p[3])), mc::Vec3i(i(p[4]), i(p[5]), i(p[6])));
            bool c = box.contains(mc::Vec3i(i(p[7]), i(p[8]), i(p[9])));
            eqI(c ? 1 : 0, p[10], line);
        }
        else if (t == "MOV") {
            // MOV  minX minY minZ  maxX maxY maxZ  dir3d  amount  nMinX nMinY nMinZ  nMaxX nMaxY nMaxZ
            mc::BlockBox box(mc::Vec3i(i(p[1]), i(p[2]), i(p[3])), mc::Vec3i(i(p[4]), i(p[5]), i(p[6])));
            mc::BlockBox mv = box.move(dirFrom3d(i(p[7])), i(p[8]));
            eqBox(mv, p[9], p[10], p[11], p[12], p[13], p[14], line);
        }
        else if (t == "EXT") {
            // EXT  minX minY minZ  maxX maxY maxZ  dir3d  amount  nMinX nMinY nMinZ  nMaxX nMaxY nMaxZ
            mc::BlockBox box(mc::Vec3i(i(p[1]), i(p[2]), i(p[3])), mc::Vec3i(i(p[4]), i(p[5]), i(p[6])));
            mc::BlockBox ex = box.extend(dirFrom3d(i(p[7])), i(p[8]));
            eqBox(ex, p[9], p[10], p[11], p[12], p[13], p[14], line);
        }
        else if (t == "OFF") {
            // OFF  minX minY minZ  maxX maxY maxZ  oX oY oZ   nMinX nMinY nMinZ  nMaxX nMaxY nMaxZ
            mc::BlockBox box(mc::Vec3i(i(p[1]), i(p[2]), i(p[3])), mc::Vec3i(i(p[4]), i(p[5]), i(p[6])));
            mc::BlockBox of = box.offset(mc::Vec3i(i(p[7]), i(p[8]), i(p[9])));
            eqBox(of, p[10], p[11], p[12], p[13], p[14], p[15], line);
        }
        else { fail("UNKNOWN_TAG " + t); }
    }

    std::cout << "BlockBox cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
