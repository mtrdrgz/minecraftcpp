// Parity test for net.minecraft.core.Vec3i (Minecraft 26.1.2). Ground truth:
// tools/Vec3iParity.java vs the real class. Ints compared exactly; doubles via
// raw bits only.
//
//   vec3i_parity --cases mcpp/build/vec3i.tsv

#include "core/Vec3i.h"

#include <bit>
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

int      i(const std::string& s) { return static_cast<int>(std::stoll(s)); }
double   bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }

mc::Direction dirFrom3d(int v) { return static_cast<mc::Direction>(v); } // DOWN=0..EAST=5
mc::Axis      axisFromOrd(int v) { return static_cast<mc::Axis>(v); }    // X=0,Y=1,Z=2

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: vec3i_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l + " got=" + std::to_string(got));
    };
    auto eqV = [&](const mc::Vec3i& got, const std::string& ex, const std::string& ey,
                   const std::string& ez, const std::string& l) {
        if (got.getX() != i(ex) || got.getY() != i(ey) || got.getZ() != i(ez))
            fail(l + " got=" + std::to_string(got.getX()) + "," + std::to_string(got.getY()) + "," + std::to_string(got.getZ()));
    };
    auto eqD = [&](double got, const std::string& exp, const std::string& l) {
        if (db(got) != std::stoull(exp, nullptr, 16)) fail(l + " gotbits=" + std::to_string(db(got)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "GET") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            if (v.getX() != i(p[4]) || v.getY() != i(p[5]) || v.getZ() != i(p[6])) fail(line);
        }
        else if (t == "GETAXIS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            if (v.get(mc::Axis::X) != i(p[4]) || v.get(mc::Axis::Y) != i(p[5]) || v.get(mc::Axis::Z) != i(p[6])) fail(line);
        }
        else if (t == "MULI") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.multiply(i(p[4])), p[5], p[6], p[7], line);
        }
        else if (t == "MUL3") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.multiply(i(p[4]), i(p[5]), i(p[6])), p[7], p[8], p[9], line);
        }
        else if (t == "ABOVE")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "BELOW")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "NORTH")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.north(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "SOUTH")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.south(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "WEST")   { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.west(i(p[4])),  p[5], p[6], p[7], line); }
        else if (t == "EAST")   { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.east(i(p[4])),  p[5], p[6], p[7], line); }
        else if (t == "ABOVE0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(), p[4], p[5], p[6], line); }
        else if (t == "BELOW0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(), p[4], p[5], p[6], line); }
        else if (t == "NORTH0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.north(), p[4], p[5], p[6], line); }
        else if (t == "SOUTH0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.south(), p[4], p[5], p[6], line); }
        else if (t == "WEST0")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.west(),  p[4], p[5], p[6], line); }
        else if (t == "EAST0")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.east(),  p[4], p[5], p[6], line); }
        else if (t == "RELDIR") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4])), i(p[5])), p[6], p[7], p[8], line);
        }
        else if (t == "RELDIR1") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4]))), p[5], p[6], p[7], line);
        }
        else if (t == "RELAXIS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(axisFromOrd(i(p[4])), i(p[5])), p[6], p[7], p[8], line);
        }
        else if (t == "OFFV") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqV(v.offset(q), p[7], p[8], p[9], line);
        }
        else if (t == "OFFI") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.offset(i(p[4]), i(p[5]), i(p[6])), p[7], p[8], p[9], line);
        }
        else if (t == "SUBV") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqV(v.subtract(q), p[7], p[8], p[9], line);
        }
        else if (t == "CROSS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqV(v.cross(q), p[7], p[8], p[9], line);
        }
        else if (t == "DISTSQR") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqD(v.distSqr(q), p[7], line);
        }
        else if (t == "DISTMAN") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.distManhattan(q), p[7], line);
        }
        else if (t == "DISTCHESS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.distChessboard(q), p[7], line);
        }
        else if (t == "DLOW") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqD(v.distToLowCornerSqr(bd(p[4]), bd(p[5]), bd(p[6])), p[7], line);
        }
        else if (t == "DCENTER") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqD(v.distToCenterSqr(bd(p[4]), bd(p[5]), bd(p[6])), p[7], line);
        }
        else if (t == "CLOSER") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.closerThan(q, bd(p[7])) ? 1 : 0, p[8], line);
        }
        else { fail("UNKNOWN_TAG " + t); }
    }

    std::cout << "Vec3i cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
