// Parity test for net.minecraft.world.phys.Vec3. Ground truth: tools/Vec3Parity.java.
// Bit-exact (doubles/floats as raw IEEE-754 bits). rotation()/atan2/asin tracked
// separately (see ROTATION) to expose any host-libm divergence.
//
//   vec3_parity --cases mcpp/build/vec3.tsv

#include "Vec3.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Vec3;
using mc::Direction;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
float  bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: vec3_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0, rotMism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };
    auto eD = [&](double got, const std::string& exp, const std::string& l) { if (db(got) != std::stoull(exp, nullptr, 16)) fail(l); };
    auto eF = [&](float got, const std::string& exp, const std::string& l) { if (fb(got) != static_cast<uint32_t>(std::stoul(exp, nullptr, 16))) fail(l); };
    auto v3 = [&](const Vec3& v, const std::vector<std::string>& p, int o, const std::string& l) {
        if (db(v.x) != std::stoull(p[o], nullptr, 16) || db(v.y) != std::stoull(p[o+1], nullptr, 16) || db(v.z) != std::stoull(p[o+2], nullptr, 16)) fail(l);
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;
        Vec3 a{bd(p[1]), bd(p[2]), bd(p[3])};

        if (t == "NORMALIZE")    v3(a.normalize(), p, 4, line);
        else if (t == "LENGTH")  eD(a.length(), p[4], line);
        else if (t == "LENGTHSQR") eD(a.lengthSqr(), p[4], line);
        else if (t == "HDIST")   eD(a.horizontalDistance(), p[4], line);
        else if (t == "REVERSE") v3(a.reverse(), p, 4, line);
        else if (t == "HORIZ")   v3(a.horizontal(), p, 4, line);
        else if (t == "ROTCW90") v3(a.rotateClockwise90(), p, 4, line);
        else if (t == "ISFINITE") { if ((a.isFinite() ? 1 : 0) != std::stoi(p[4])) fail(line); }
        else if (t == "SCALE")   v3(a.scale(bd(p[4])), p, 5, line);
        else if (t == "GET")     eD(a.get(std::stoi(p[4])), p[5], line);
        else if (t == "WITH")    v3(a.with(std::stoi(p[4]), bd(p[5])), p, 6, line);
        else if (t == "RELATIVE") v3(a.relative((Direction)std::stoi(p[4]), bd(p[5])), p, 6, line);
        else if (t == "XROT")    v3(a.xRot(bf(p[4])), p, 5, line);
        else if (t == "YROT")    v3(a.yRot(bf(p[4])), p, 5, line);
        else if (t == "ZROT")    v3(a.zRot(bf(p[4])), p, 5, line);
        else if (t == "ROTATION") {
            float pitch, yaw; a.rotation(pitch, yaw);
            if (fb(pitch) != static_cast<uint32_t>(std::stoul(p[4], nullptr, 16)) || fb(yaw) != static_cast<uint32_t>(std::stoul(p[5], nullptr, 16))) {
                ++rotMism; if (shown++ < 30) std::cerr << "ROTATION-DIFF (atan2/asin libm) " << line << "\n";
            }
        }
        else if (t == "ADD" || t == "SUB" || t == "MUL" || t == "DOT" || t == "CROSS" || t == "VECTORTO" || t == "DISTSQR" || t == "DISTTO" || t == "PROJ" || t == "LERP") {
            Vec3 b{bd(p[4]), bd(p[5]), bd(p[6])};
            if (t == "ADD") v3(a.add(b), p, 7, line);
            else if (t == "SUB") v3(a.subtract(b), p, 7, line);
            else if (t == "MUL") v3(a.multiply(b), p, 7, line);
            else if (t == "DOT") eD(a.dot(b), p[7], line);
            else if (t == "CROSS") v3(a.cross(b), p, 7, line);
            else if (t == "VECTORTO") v3(a.vectorTo(b), p, 7, line);
            else if (t == "DISTSQR") eD(a.distanceToSqr(b), p[7], line);
            else if (t == "DISTTO") eD(a.distanceTo(b), p[7], line);
            else if (t == "PROJ") v3(a.projectedOn(b), p, 7, line);
            else if (t == "LERP") v3(a.lerp(b, bd(p[7])), p, 8, line);
        }
        else if (t == "DIRFROMROT") v3(Vec3::directionFromRotation(bf(p[1]), bf(p[2])), p, 3, line);
        else if (t == "APPLYLOCAL") {
            Vec3 dir{bd(p[3]), bd(p[4]), bd(p[5])};
            v3(Vec3::applyLocalCoordinatesToRotation(bf(p[1]), bf(p[2]), dir), p, 6, line);
        }
        else fail("UNKNOWN_TAG " + t);
    }

    std::cout << "Vec3 cases=" << total << " mismatches=" << mism << " rotation_libm_diffs=" << rotMism << "\n";
    return (mism == 0 && rotMism == 0) ? 0 : 1;
}
