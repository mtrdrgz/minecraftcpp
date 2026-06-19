// VERIFY-existing parity test for net.minecraft.core.Vec3i (Minecraft 26.1.2).
//
// The Vec3i surface called out by the assignment is already ported & certified in
// core/Vec3i.h — this test #includes it and re-verifies it BIT-FOR-BIT against
// ground truth from the REAL class (tools/Vec3iVerifyParity.java):
//
//   getX/getY/getZ                       -> mc::Vec3i::getX/getY/getZ
//   above()/above(steps)                 -> mc::Vec3i::above
//   below()/below(steps)                 -> mc::Vec3i::below
//   relative(Direction[,steps])          -> mc::Vec3i::relative
//   cross(Vec3i)                         -> mc::Vec3i::cross
//   distSqr(Vec3i)        (double)       -> mc::Vec3i::distSqr   (bit-exact, %016x)
//   distManhattan(Vec3i)  (int)          -> mc::Vec3i::distManhattan
//   distChessboard(Vec3i) (int)          -> mc::Vec3i::distChessboard
//   closerThan(Vec3i,double) (bool)      -> mc::Vec3i::closerThan
//   compareTo(Vec3i)      (int)          -> mc::vec3i_verify::compareTo (test-local
//                                           1:1 port; the engine header omits the
//                                           Comparable surface)
//
// distSqr returns a double -> reconstructed from raw IEEE-754 bits and compared
// with std::bit_cast (bit-for-bit). closerThan's distance is a double, fed in as
// raw bits so the EXACT double round-trips. Everything else is integer-exact.
//
//   vec3i_verify_parity --cases mcpp/build/vec3i_verify.tsv

#include "core/Vec3i.h"
#include "world/phys/Direction.h"
#include "world/level/levelgen/Vec3iCompareTo.h"

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

int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Parse a 16-hex-digit raw IEEE-754 double (Double.doubleToRawLongBits).
double dbits(const std::string& s) {
    return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s, nullptr, 16)));
}

mc::Direction dirFrom3d(int v) { return static_cast<mc::Direction>(v); } // DOWN=0..EAST=5

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: vec3i_verify_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l_, const std::string& got) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l_ << " got=" << got << "\n";
    };
    auto eqI = [&](int32_t got, const std::string& exp, const std::string& ln) {
        if (got != i(exp)) fail(ln, std::to_string(got));
    };
    auto eqV = [&](const mc::Vec3i& v, const std::string& ex, const std::string& ey,
                   const std::string& ez, const std::string& ln) {
        if (v.getX() != i(ex) || v.getY() != i(ey) || v.getZ() != i(ez))
            fail(ln, std::to_string(v.getX()) + "," + std::to_string(v.getY()) + "," + std::to_string(v.getZ()));
    };
    // bit-for-bit double compare (recompute -> bits, expected from %016x string).
    auto eqD = [&](double got, const std::string& exp, const std::string& ln) {
        if (std::bit_cast<uint64_t>(got) != static_cast<uint64_t>(std::stoull(exp, nullptr, 16)))
            fail(ln, std::to_string(std::bit_cast<uint64_t>(got)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "V3_GET") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            // getX/getY/getZ
            if (v.getX() != i(p[4]) || v.getY() != i(p[5]) || v.getZ() != i(p[6]))
                fail(line, std::to_string(v.getX()) + "," + std::to_string(v.getY()) + "," + std::to_string(v.getZ()));
        }
        else if (t == "V3_ABOVE0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(), p[4], p[5], p[6], line); }
        else if (t == "V3_BELOW0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(), p[4], p[5], p[6], line); }
        else if (t == "V3_ABOVE")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "V3_BELOW")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "V3_RELDIR1") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4]))), p[5], p[6], p[7], line);
        }
        else if (t == "V3_RELDIR") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4])), i(p[5])), p[6], p[7], p[8], line);
        }
        else if (t == "V3_CMP") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(mc::vec3i_verify::compareTo(v, q), p[7], line);
        }
        else if (t == "V3_CROSS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqV(v.cross(q), p[7], p[8], p[9], line);
        }
        else if (t == "V3_DISTSQR") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqD(v.distSqr(q), p[7], line);
        }
        else if (t == "V3_DISTMAN") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.distManhattan(q), p[7], line);
        }
        else if (t == "V3_DISTCHESS") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.distChessboard(q), p[7], line);
        }
        else if (t == "V3_CLOSER") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            const double dist = dbits(p[7]);
            const int got = v.closerThan(q, dist) ? 1 : 0;
            eqI(got, p[8], line);
        }
        else { fail("UNKNOWN_TAG " + t, t); }
    }

    std::cout << "Vec3iVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
