// VERIFY-existing parity test for net.minecraft.core.BlockPos (Minecraft 26.1.2).
//
// BlockPos's whole surface called out by the assignment is already ported &
// certified across two engine headers — this test #includes them and re-verifies
// them BIT-FOR-BIT against ground truth from the REAL class
// (tools/BlockPosVerifyParity.java):
//
//   static asLong/getX/getY/getZ/of/offset(long..)/getFlatIndex
//        -> mc::poscodec::*                                  (core/PosCodec.h)
//   instance offset(int,int,int) / relative(Direction[,steps]) /
//   above/below/north/south/west/east([steps]) / distManhattan(Vec3i)
//        -> mc::Vec3i::*  (BlockPos extends Vec3i and these methods compute the
//           identical coordinate values: BlockPos.offset==Vec3i.offset value-wise,
//           BlockPos.relative uses Direction.getStepX/Y/Z exactly as Vec3i, and
//           distManhattan is inherited)                      (core/Vec3i.h)
//
// Ints/longs compared exactly. No floats funnel through here, so every compare is
// integer-exact.
//
//   block_pos_verify_parity --cases mcpp/build/block_pos_verify.tsv

#include "core/PosCodec.h"
#include "core/Vec3i.h"
#include "world/phys/Direction.h"

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
int64_t  l(const std::string& s) { return static_cast<int64_t>(std::stoll(s)); }

mc::Direction dirFrom3d(int v) { return static_cast<mc::Direction>(v); } // DOWN=0..EAST=5

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_pos_verify_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l_, const std::string& got) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l_ << " got=" << got << "\n";
    };
    auto eqL = [&](int64_t got, const std::string& exp, const std::string& ln) {
        if (got != l(exp)) fail(ln, std::to_string(got));
    };
    auto eqI = [&](int32_t got, const std::string& exp, const std::string& ln) {
        if (got != i(exp)) fail(ln, std::to_string(got));
    };
    auto eqV = [&](const mc::Vec3i& v, const std::string& ex, const std::string& ey,
                   const std::string& ez, const std::string& ln) {
        if (v.getX() != i(ex) || v.getY() != i(ey) || v.getZ() != i(ez))
            fail(ln, std::to_string(v.getX()) + "," + std::to_string(v.getY()) + "," + std::to_string(v.getZ()));
    };
    // Verify a packed-long triplet against poscodec getX/getY/getZ.
    auto eqUnpack = [&](int64_t node, const std::string& ex, const std::string& ey,
                        const std::string& ez, const std::string& ln) {
        if (mc::poscodec::blockPosGetX(node) != i(ex) ||
            mc::poscodec::blockPosGetY(node) != i(ey) ||
            mc::poscodec::blockPosGetZ(node) != i(ez))
            fail(ln, std::to_string(mc::poscodec::blockPosGetX(node)) + "," +
                     std::to_string(mc::poscodec::blockPosGetY(node)) + "," +
                     std::to_string(mc::poscodec::blockPosGetZ(node)));
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        // ── static long packing ─────────────────────────────────────────────
        if (t == "BP_ASLONG") {
            // asLong(x,y,z) -> node
            eqL(mc::poscodec::blockPosAsLong(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        }
        else if (t == "BP_GET") {
            // getX/getY/getZ(node)
            eqUnpack(l(p[1]), p[2], p[3], p[4], line);
        }
        else if (t == "BP_OF") {
            // of(node).getX/getY/getZ — same unpack as BP_GET
            eqUnpack(l(p[1]), p[2], p[3], p[4], line);
        }
        else if (t == "BP_FLAT") {
            eqL(mc::poscodec::blockPosGetFlatIndex(l(p[1])), p[2], line);
        }
        else if (t == "BP_OFFSETL") {
            // offset(node, sx, sy, sz)
            eqL(mc::poscodec::blockPosOffset(l(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        }
        else if (t == "BP_OFFSETD") {
            // offset(node, Direction) == offset(node, stepX, stepY, stepZ)
            const int d = i(p[2]);
            const int sx = mc::DIRECTION_NORMAL[d][0];
            const int sy = mc::DIRECTION_NORMAL[d][1];
            const int sz = mc::DIRECTION_NORMAL[d][2];
            eqL(mc::poscodec::blockPosOffset(l(p[1]), sx, sy, sz), p[3], line);
        }
        // ── instance offset/relative/above…east (value parity via Vec3i) ────
        else if (t == "BP_OFFI") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.offset(i(p[4]), i(p[5]), i(p[6])), p[7], p[8], p[9], line);
        }
        else if (t == "BP_RELDIR1") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4]))), p[5], p[6], p[7], line);
        }
        else if (t == "BP_RELDIR") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3]));
            eqV(v.relative(dirFrom3d(i(p[4])), i(p[5])), p[6], p[7], p[8], line);
        }
        else if (t == "BP_ABOVE0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(), p[4], p[5], p[6], line); }
        else if (t == "BP_BELOW0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(), p[4], p[5], p[6], line); }
        else if (t == "BP_NORTH0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.north(), p[4], p[5], p[6], line); }
        else if (t == "BP_SOUTH0") { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.south(), p[4], p[5], p[6], line); }
        else if (t == "BP_WEST0")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.west(),  p[4], p[5], p[6], line); }
        else if (t == "BP_EAST0")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.east(),  p[4], p[5], p[6], line); }
        else if (t == "BP_ABOVE")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.above(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "BP_BELOW")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.below(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "BP_NORTH")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.north(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "BP_SOUTH")  { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.south(i(p[4])), p[5], p[6], p[7], line); }
        else if (t == "BP_WEST")   { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.west(i(p[4])),  p[5], p[6], p[7], line); }
        else if (t == "BP_EAST")   { mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])); eqV(v.east(i(p[4])),  p[5], p[6], p[7], line); }
        else if (t == "BP_DISTMAN") {
            mc::Vec3i v(i(p[1]), i(p[2]), i(p[3])), q(i(p[4]), i(p[5]), i(p[6]));
            eqI(v.distManhattan(q), p[7], line);
        }
        else { fail("UNKNOWN_TAG " + t, t); }
    }

    std::cout << "BlockPosVerify cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
