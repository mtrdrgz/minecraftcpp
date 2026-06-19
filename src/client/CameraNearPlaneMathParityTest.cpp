// Parity test for client/CameraNearPlaneMath.h — the pure near-plane geometry of
// net.minecraft.client.Camera + Camera.NearPlane (MC 26.1.2). Ground truth:
// tools/CameraNearPlaneMathParity.java (drives the REAL Camera#setRotation,
// Camera#getNearPlane and Camera.NearPlane getters via reflection over the shipped
// client.jar + joml-1.10.8.jar).
//
// Row types (all coordinate fields are doubles = %016x; the key + plane coords are
// floats = %08x):
//   NEARPLANE <yRot> <xRot> <fov> <w> <h> <zNear>  <forward.xyz> <left.xyz> <up.xyz>
//   CORNERS   <... key ...>                         <TL.xyz> <TR.xyz> <BL.xyz> <BR.xyz>
//   POINT     <... key ...> <px> <py>               <point.xyz>
//
// Doubles compared as raw IEEE-754 bits; the basis comes from the certified
// setRotation port (CameraRotationMath.h), and the tan() path uses the same
// libm convention the real Math.tan(double) uses on this platform.
//
//   camera_nearplane_math_parity --cases mcpp/build/camera_nearplane_math.tsv

#include "CameraNearPlaneMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cn = mc::client::camera_nearplane;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
double bd(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16))); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: camera_nearplane_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l, const char* tag) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << " @" << tag << "\n";
    };
    // Compare expected raw-hex double at field offset o against got double.
    auto eqAt = [&](double got, const std::vector<std::string>& p, int o, const std::string& l, const char* tag) {
        if (db(got) != static_cast<uint64_t>(std::stoull(p[o], nullptr, 16))) { fail(l, tag); return false; }
        return true;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "NEARPLANE" || t == "CORNERS" || t == "POINT") {
            // Key: p[1..6] = yRot, xRot, fov, w, h, zNear (floats).
            float yRot = bf(p[1]), xRot = bf(p[2]), fov = bf(p[3]);
            float w = bf(p[4]), h = bf(p[5]), zNear = bf(p[6]);
            cn::NearPlane plane = cn::getNearPlaneFromRotation(yRot, xRot, fov, w, h, zNear);

            if (t == "NEARPLANE") {
                // p[7..9]=forward.xyz ; p[10..12]=left.xyz ; p[13..15]=up.xyz
                eqAt(plane.forward.x, p, 7, line, "fwd.x") && eqAt(plane.forward.y, p, 8, line, "fwd.y")
                    && eqAt(plane.forward.z, p, 9, line, "fwd.z")
                    && eqAt(plane.left.x, p, 10, line, "left.x") && eqAt(plane.left.y, p, 11, line, "left.y")
                    && eqAt(plane.left.z, p, 12, line, "left.z")
                    && eqAt(plane.up.x, p, 13, line, "up.x") && eqAt(plane.up.y, p, 14, line, "up.y")
                    && eqAt(plane.up.z, p, 15, line, "up.z");
            } else if (t == "CORNERS") {
                // p[7..9]=TL ; p[10..12]=TR ; p[13..15]=BL ; p[16..18]=BR
                cn::Vec3 tl = plane.getTopLeft();
                cn::Vec3 tr = plane.getTopRight();
                cn::Vec3 bl = plane.getBottomLeft();
                cn::Vec3 br = plane.getBottomRight();
                eqAt(tl.x, p, 7, line, "tl.x") && eqAt(tl.y, p, 8, line, "tl.y") && eqAt(tl.z, p, 9, line, "tl.z")
                    && eqAt(tr.x, p, 10, line, "tr.x") && eqAt(tr.y, p, 11, line, "tr.y") && eqAt(tr.z, p, 12, line, "tr.z")
                    && eqAt(bl.x, p, 13, line, "bl.x") && eqAt(bl.y, p, 14, line, "bl.y") && eqAt(bl.z, p, 15, line, "bl.z")
                    && eqAt(br.x, p, 16, line, "br.x") && eqAt(br.y, p, 17, line, "br.y") && eqAt(br.z, p, 18, line, "br.z");
            } else { // POINT
                // p[7]=px ; p[8]=py ; p[9..11]=point.xyz
                float px = bf(p[7]), py = bf(p[8]);
                cn::Vec3 pt = plane.getPointOnPlane(px, py);
                eqAt(pt.x, p, 9, line, "pt.x") && eqAt(pt.y, p, 10, line, "pt.y") && eqAt(pt.z, p, 11, line, "pt.z");
            }
        } else {
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "CameraNearPlaneMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
