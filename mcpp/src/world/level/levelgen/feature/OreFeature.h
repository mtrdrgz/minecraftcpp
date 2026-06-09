#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.OreFeature (+ canPlaceOre /
// isAdjacentToAir / shouldSkipAirCheck) and OreConfiguration. RNG order MUST match Java:
//   dir = nextFloat()*PI ; y0 = originY + nextInt(3)-2 ; y1 = originY + nextInt(3)-2 ;
//   per segment ss = nextDouble()*size/16 ; per accepted cell canPlaceOre runs the
//   RuleTest(s) (random_block_match consumes nextFloat) then the air-exposure check
//   (consumes nextFloat only when 0 < discard < 1).
// NOTE the spread axis (x0/x1/z0/z1) uses REAL std::sin/std::cos (Java Math.sin/cos),
// while the per-segment radius uses the TABLE-based Mth.sin — do not confuse them.
//
// The RuleTest is supplied as a resolved functor (state-id-string + RandomSource ->
// bool) so this header stays decoupled from the block-tag subsystem; the caller builds
// it (tag_match via block tags, block_match/random_block_match by id).
//
// Certification: ore spills across chunk borders, so it can only be byte-matched in a
// real 3x3 WorldGenRegion against the server (NOT the single-chunk decoration harness,
// whose Proxy wraps out-of-chunk writes). See mcpp/docs/DECORATION_PLAN.md.

#include "../placement/PlacementContext.h"   // WorldGenLevel
#include "../placement/PlacedFeature.h"       // FeaturePlacer
#include "../RandomSource.h"
#include "../Mth.h"
#include "../Heightmap.h"
#include "../../chunk/LevelChunk.h"           // CHUNK_MIN_Y / CHUNK_MAX_Y
#include "../../../../core/Math.h"            // BlockPos

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::Heightmap;
using mc::levelgen::placement::WorldGenLevel;
namespace mth = mc::levelgen::mth;

// RuleTest resolved to a predicate over the current block-state id + the RNG.
using OreRuleTest = std::function<bool(const std::string& state, RandomSource& random)>;

struct OreTarget {
    OreRuleTest target;   // RuleTest.test(state, random)
    std::string state;    // the ore BlockState id to place ("minecraft:...")
};

// Feature.isAdjacentToAir: any of the 6 face-neighbours is air.
inline bool oreIsAdjacentToAir(WorldGenLevel& level, BlockPos p) {
    static const int dirs[6][3] = { {0,-1,0},{0,1,0},{0,0,-1},{0,0,1},{-1,0,0},{1,0,0} };
    for (const auto& d : dirs)
        if (level.getBlockState(BlockPos{ p.x + d[0], p.y + d[1], p.z + d[2] }) == "minecraft:air")
            return true;
    return false;
}

inline bool oreShouldSkipAirCheck(RandomSource& random, float discardChanceOnAirExposure) {
    if (discardChanceOnAirExposure <= 0.0f) return true;
    if (discardChanceOnAirExposure >= 1.0f) return false;
    return random.nextFloat() >= discardChanceOnAirExposure;
}

inline bool oreCanPlace(const std::string& orePosState, WorldGenLevel& level, RandomSource& random,
                        float discardChanceOnAirExposure, const OreTarget& target, BlockPos orePos) {
    if (!target.target(orePosState, random)) return false;
    return oreShouldSkipAirCheck(random, discardChanceOnAirExposure) ? true : !oreIsAdjacentToAir(level, orePos);
}

// Build the OreFeature placer for a parsed OreConfiguration.
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeOrePlacer(
        std::vector<OreTarget> targets, int size, float discardChanceOnAirExposure) {
    auto tg = std::make_shared<std::vector<OreTarget>>(std::move(targets));
    return [tg, size, discardChanceOnAirExposure](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        constexpr float PI_F = 3.14159265358979323846f;  // (float)Math.PI
        const float dir = random.nextFloat() * PI_F;
        const float spreadXY = size / 8.0f;
        const int maxRadius = mth::ceil((size / 16.0f * 2.0f + 1.0f) / 2.0f);
        const double sinDir = std::sin(static_cast<double>(dir));   // Math.sin (real)
        const double cosDir = std::cos(static_cast<double>(dir));   // Math.cos (real)
        const double x0 = origin.x + sinDir * spreadXY;
        const double x1 = origin.x - sinDir * spreadXY;
        const double z0 = origin.z + cosDir * spreadXY;
        const double z1 = origin.z - cosDir * spreadXY;
        const double y0 = origin.y + random.nextInt(3) - 2;
        const double y1 = origin.y + random.nextInt(3) - 2;
        const int xStart = origin.x - mth::ceil(spreadXY) - maxRadius;
        const int yStart = origin.y - 2 - maxRadius;
        const int zStart = origin.z - mth::ceil(spreadXY) - maxRadius;
        const int sizeXZ = 2 * (mth::ceil(spreadXY) + maxRadius);
        const int sizeY  = 2 * (2 + maxRadius);

        bool found = false;
        for (int xp = xStart; xp <= xStart + sizeXZ && !found; ++xp)
            for (int zp = zStart; zp <= zStart + sizeXZ; ++zp)
                if (yStart <= level.getHeight(Heightmap::Types::OCEAN_FLOOR_WG, xp, zp)) { found = true; break; }
        if (!found) return false;

        std::vector<double> data(static_cast<std::size_t>(size) * 4);
        for (int i = 0; i < size; ++i) {
            const float step = static_cast<float>(i) / size;
            const double xx = mth::lerp(step, x0, x1);
            const double yy = mth::lerp(step, y0, y1);
            const double zz = mth::lerp(step, z0, z1);
            const double ss = random.nextDouble() * size / 16.0;
            const double r = ((mth::sin(static_cast<double>(PI_F * step)) + 1.0f) * ss + 1.0) / 2.0;
            data[i * 4 + 0] = xx; data[i * 4 + 1] = yy; data[i * 4 + 2] = zz; data[i * 4 + 3] = r;
        }
        for (int i1 = 0; i1 < size - 1; ++i1) {
            if (data[i1 * 4 + 3] <= 0.0) continue;
            for (int i2 = i1 + 1; i2 < size; ++i2) {
                if (data[i2 * 4 + 3] <= 0.0) continue;
                const double dx = data[i1*4]   - data[i2*4];
                const double dy = data[i1*4+1] - data[i2*4+1];
                const double dz = data[i1*4+2] - data[i2*4+2];
                const double dr = data[i1*4+3] - data[i2*4+3];
                if (dr * dr > dx*dx + dy*dy + dz*dz) {
                    if (dr > 0.0) data[i2*4+3] = -1.0; else data[i1*4+3] = -1.0;
                }
            }
        }

        std::unordered_set<long long> tested;   // Java BitSet (grows; non-negative indices)
        int placed = 0;
        for (int i = 0; i < size; ++i) {
            const double r = data[i*4+3];
            if (r < 0.0) continue;
            const double xx = data[i*4], yy = data[i*4+1], zz = data[i*4+2];
            const int xMin = std::max(mth::floor(xx - r), xStart);
            const int yMin = std::max(mth::floor(yy - r), yStart);
            const int zMin = std::max(mth::floor(zz - r), zStart);
            const int xMax = std::max(mth::floor(xx + r), xMin);
            const int yMax = std::max(mth::floor(yy + r), yMin);
            const int zMax = std::max(mth::floor(zz + r), zMin);
            for (int x = xMin; x <= xMax; ++x) {
                const double xd = (x + 0.5 - xx) / r;
                if (xd * xd >= 1.0) continue;
                for (int y = yMin; y <= yMax; ++y) {
                    const double yd = (y + 0.5 - yy) / r;
                    if (xd*xd + yd*yd >= 1.0) continue;
                    for (int z = zMin; z <= zMax; ++z) {
                        const double zd = (z + 0.5 - zz) / r;
                        if (xd*xd + yd*yd + zd*zd >= 1.0) continue;
                        if (y < mc::CHUNK_MIN_Y || y >= mc::CHUNK_MAX_Y) continue;  // isOutsideBuildHeight
                        const long long bitIndex = static_cast<long long>(x - xStart)
                            + static_cast<long long>(y - yStart) * sizeXZ
                            + static_cast<long long>(z - zStart) * sizeXZ * sizeY;
                        if (!tested.insert(bitIndex).second) continue;
                        const BlockPos orePos{ x, y, z };
                        if (!level.ensureCanWrite(orePos)) continue;
                        const std::string cur = level.getBlockState(orePos);
                        for (const auto& t : *tg) {
                            if (oreCanPlace(cur, level, random, discardChanceOnAirExposure, t, orePos)) {
                                level.setBlock(orePos, t.state, 2);
                                ++placed;
                                break;
                            }
                        }
                    }
                }
            }
        }
        return placed > 0;
    };
}

} // namespace mc::levelgen::feature
