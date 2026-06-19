// Parity test for net.minecraft.world.phys.HitResult / BlockHitResult /
// EntityHitResult. Ground truth: tools/HitResultParity.java. Pure geometry, so
// bit-exact (doubles as raw IEEE-754 bits; ints/booleans/ordinals as decimal).
//
//   hit_result_parity --cases mcpp/build/hit_result.tsv

#include "HitResult.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::BlockHitResult;
using mc::BlockPos;
using mc::Direction;
using mc::EntityHitResult;
using mc::HitResultType;
using mc::Vec3;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
double bd(const std::string& s) { return std::bit_cast<double>(std::stoull(s, nullptr, 16)); }
uint64_t db(double v) { return std::bit_cast<uint64_t>(v); }
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: hit_result_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };

    auto eqD = [&](double got, const std::string& exp, const std::string& l) {
        if (db(got) != std::stoull(exp, nullptr, 16)) fail(l);
    };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) {
        if (got != std::stoll(exp)) fail(l);
    };
    // Compare an emitted block-result tail: typeOrd dir bx by bz locX locY locZ inside wbh.
    auto eqBlock = [&](const BlockHitResult& r, const std::vector<std::string>& p, int o,
                       const std::string& l) {
        if ((long long)r.getType() != std::stoll(p[o])) { fail(l); return; }
        if ((long long)r.getDirection() != std::stoll(p[o + 1])) { fail(l); return; }
        if (r.getBlockPos().x != std::stoi(p[o + 2]) || r.getBlockPos().y != std::stoi(p[o + 3])
            || r.getBlockPos().z != std::stoi(p[o + 4])) { fail(l); return; }
        if (db(r.getLocation().x) != std::stoull(p[o + 5], nullptr, 16)
            || db(r.getLocation().y) != std::stoull(p[o + 6], nullptr, 16)
            || db(r.getLocation().z) != std::stoull(p[o + 7], nullptr, 16)) { fail(l); return; }
        if ((int)r.isInside() != std::stoi(p[o + 8])) { fail(l); return; }
        if ((int)r.isWorldBorderHit() != std::stoi(p[o + 9])) { fail(l); return; }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "DISTANCETO") {
            // p: tag locX locY locZ posX posY posZ -> distSqr
            Vec3 loc{bd(p[1]), bd(p[2]), bd(p[3])};
            Vec3 pos{bd(p[4]), bd(p[5]), bd(p[6])};
            BlockHitResult r(loc, Direction::DOWN, BlockPos{0, 0, 0}, false);
            eqD(r.distanceTo(pos), p[7], line);
            continue;
        }
        if (t == "ENTITY_GETLOC") {
            Vec3 loc{bd(p[1]), bd(p[2]), bd(p[3])};
            EntityHitResult e(loc);
            const Vec3& g = e.getLocation();
            if (db(g.x) != std::stoull(p[4], nullptr, 16)
                || db(g.y) != std::stoull(p[5], nullptr, 16)
                || db(g.z) != std::stoull(p[6], nullptr, 16)) fail(line);
            continue;
        }
        if (t == "ENTITY_GETTYPE") {
            Vec3 loc{bd(p[1]), bd(p[2]), bd(p[3])};
            EntityHitResult e(loc);
            eqI((long long)e.getType(), p[4], line);
            continue;
        }
        if (t == "MISS") {
            // p: tag locX locY locZ dirOrd bx by bz | typeOrd dir bx by bz loc.. inside wbh
            Vec3 loc{bd(p[1]), bd(p[2]), bd(p[3])};
            Direction dir = (Direction)std::stoi(p[4]);
            BlockPos pos{std::stoi(p[5]), std::stoi(p[6]), std::stoi(p[7])};
            BlockHitResult r = BlockHitResult::missOf(loc, dir, pos);
            eqBlock(r, p, 8, line);
            continue;
        }

        // All remaining tags share the leading block input:
        //   tag locX locY locZ dirOrd bx by bz inside wbh ...
        Vec3 loc{bd(p[1]), bd(p[2]), bd(p[3])};
        Direction dir = (Direction)std::stoi(p[4]);
        BlockPos pos{std::stoi(p[5]), std::stoi(p[6]), std::stoi(p[7])};
        bool inside = std::stoi(p[8]) != 0;
        bool wbh = std::stoi(p[9]) != 0;
        BlockHitResult r(loc, dir, pos, inside, wbh);

        if (t == "GETLOCATION") {
            const Vec3& g = r.getLocation();
            if (db(g.x) != std::stoull(p[10], nullptr, 16)
                || db(g.y) != std::stoull(p[11], nullptr, 16)
                || db(g.z) != std::stoull(p[12], nullptr, 16)) fail(line);
        } else if (t == "GETBLOCKPOS") {
            const BlockPos& g = r.getBlockPos();
            if (g.x != std::stoi(p[10]) || g.y != std::stoi(p[11]) || g.z != std::stoi(p[12])) fail(line);
        } else if (t == "GETDIRECTION") {
            eqI((long long)r.getDirection(), p[10], line);
        } else if (t == "GETTYPE") {
            eqI((long long)r.getType(), p[10], line);
        } else if (t == "ISINSIDE") {
            eqI((int)r.isInside(), p[10], line);
        } else if (t == "ISWBH") {
            eqI((int)r.isWorldBorderHit(), p[10], line);
        } else if (t == "WITHDIR") {
            Direction nd = (Direction)std::stoi(p[10]);
            eqBlock(r.withDirection(nd), p, 11, line);
        } else if (t == "WITHPOS") {
            BlockPos np{std::stoi(p[10]), std::stoi(p[11]), std::stoi(p[12])};
            eqBlock(r.withPosition(np), p, 13, line);
        } else if (t == "HITBORDER") {
            eqBlock(r.hitBorder(), p, 10, line);
        } else {
            std::cerr << "unknown tag: " << t << "\n";
            fail(line);
        }
    }

    std::cout << "HitResult cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
