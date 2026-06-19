// Parity test for client/CameraRotationMath.h — the pure orientation math of
// net.minecraft.client.Camera#setRotation (MC 26.1.2). Ground truth:
// tools/CameraRotationMathParity.java (drives the REAL org.joml.Quaternionf /
// Vector3f from joml-1.10.8.jar and reproduces the setRotation body).
//
// Three row types:
//   ROTYXZ  <angleY> <angleX> <angleZ>  <qx qy qz qw>
//   VROTATE <qx qy qz qw> <vx vy vz>     <rx ry rz>
//   SETROT  <yRotDeg> <xRotDeg>          <qx qy qz qw> <fx fy fz> <ux uy uz> <lx ly lz>
//
// Floats compared as raw IEEE-754 bits. rotationYXZ uses the libm sin path; it
// matches the certified (float)std::sin((double)x) convention exactly like the
// QuaternionMath.h rotationXYZ rows.
//
//   camera_rotation_math_parity --cases mcpp/build/camera_rotation_math.tsv

#include "CameraRotationMath.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cr = mc::client::camera_rot;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: camera_rotation_math_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 30) std::cerr << "MISMATCH " << l << "\n"; };

    // Compare expected raw-hex at field offset o against got float.
    auto eqAt = [&](float got, const std::vector<std::string>& p, int o, const std::string& l, const char* tag) {
        if (fb(got) != static_cast<uint32_t>(std::stoul(p[o], nullptr, 16))) { fail(l + " " + tag); return false; }
        return true;
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ROTYXZ") {
            // p[1..3]=angleY,angleX,angleZ ; p[4..7]=qx,qy,qz,qw
            cr::Quaternionf q; q.rotationYXZ(bf(p[1]), bf(p[2]), bf(p[3]));
            eqAt(q.x, p, 4, line, "x") && eqAt(q.y, p, 5, line, "y")
                && eqAt(q.z, p, 6, line, "z") && eqAt(q.w, p, 7, line, "w");
        } else if (t == "VROTATE") {
            // p[1..4]=qx,qy,qz,qw ; p[5..7]=vx,vy,vz ; p[8..10]=rx,ry,rz
            cr::Quaternionf q; q.x = bf(p[1]); q.y = bf(p[2]); q.z = bf(p[3]); q.w = bf(p[4]);
            cr::Vector3f vec(bf(p[5]), bf(p[6]), bf(p[7]));
            cr::Vector3f r = vec.rotateInto(q);
            eqAt(r.x, p, 8, line, "x") && eqAt(r.y, p, 9, line, "y") && eqAt(r.z, p, 10, line, "z");
        } else if (t == "SETROT") {
            // p[1..2]=yRot,xRot ; p[3..6]=q ; p[7..9]=fwd ; p[10..12]=up ; p[13..15]=left
            cr::CameraOrientation o = cr::setRotation(bf(p[1]), bf(p[2]));
            eqAt(o.rotation.x, p, 3, line, "qx") && eqAt(o.rotation.y, p, 4, line, "qy")
                && eqAt(o.rotation.z, p, 5, line, "qz") && eqAt(o.rotation.w, p, 6, line, "qw")
                && eqAt(o.forwards.x, p, 7, line, "fx") && eqAt(o.forwards.y, p, 8, line, "fy")
                && eqAt(o.forwards.z, p, 9, line, "fz")
                && eqAt(o.up.x, p, 10, line, "ux") && eqAt(o.up.y, p, 11, line, "uy")
                && eqAt(o.up.z, p, 12, line, "uz")
                && eqAt(o.left.x, p, 13, line, "lx") && eqAt(o.left.y, p, 14, line, "ly")
                && eqAt(o.left.z, p, 15, line, "lz");
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "CameraRotationMath checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
