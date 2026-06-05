#include "TreeGen.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"
#include "../RandomSource.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace mc::levelgen::feature {

// ==========================================================================
// TreeWorld helpers
// ==========================================================================

bool TreeWorld::validTreePos(int wx, int wy, int wz) const {
    uint32_t s = getBlock(wx, wy, wz);
    if (s == 0) return true;
    const BlockState* bs = getBlockState(s);
    if (!bs || !bs->block) return true;
    // Replaceable: non-opaque, non-fluid (catches leaves, short grass, etc.)
    return !bs->block->isOpaque() && !bs->block->isFluid();
}

bool TreeWorld::isFree(int wx, int wy, int wz) const {
    if (validTreePos(wx, wy, wz)) return true;
    // Also free if it's any log block (allows branches to pass through trunk)
    uint32_t s = getBlock(wx, wy, wz);
    if (s == 0) return true;
    const BlockState* bs = getBlockState(s);
    if (!bs || !bs->block) return true;
    return bs->block->name.find("log") != std::string::npos ||
           bs->block->name.find("stem") != std::string::npos;
}

// ==========================================================================
// FoliagePlacer base
// ==========================================================================

void FoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                    const TreeConfig& config,
                                    int treeHeight, const FoliageAttachment& att,
                                    int foliageHeight, int leafRadius) {
    int offsetSample = offset.sample(rng);
    createFoliage(world, rng, config, treeHeight, att, foliageHeight, leafRadius, offsetSample);
}

void FoliagePlacer::placeLeavesRow(TreeWorld& world, RandomSource& rng,
                                    const TreeConfig& config,
                                    int ox, int oy, int oz,
                                    int currentRadius, int dy, bool doubleTrunk) {
    int extra = doubleTrunk ? 1 : 0;
    for (int dx = -currentRadius; dx <= currentRadius + extra; ++dx) {
        for (int dz = -currentRadius; dz <= currentRadius + extra; ++dz) {
            int adx = doubleTrunk ? std::min(std::abs(dx), std::abs(dx - 1)) : std::abs(dx);
            int adz = doubleTrunk ? std::min(std::abs(dz), std::abs(dz - 1)) : std::abs(dz);
            if (!shouldSkipLocation(rng, adx, dy, adz, currentRadius, doubleTrunk)) {
                uint32_t cur = world.getBlock(ox + dx, oy + dy, oz + dz);
                // Only place if valid tree position (mirrors FoliagePlacer.tryPlaceLeaf)
                if (world.validTreePos(ox + dx, oy + dy, oz + dz)) {
                    world.setBlock(ox + dx, oy + dy, oz + dz, config.leavesStateId);
                }
            }
        }
    }
}

// ==========================================================================
// BlobFoliagePlacer
// ==========================================================================

void BlobFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                        const TreeConfig& config,
                                        int /*treeHeight*/, const FoliageAttachment& att,
                                        int foliageHeight, int leafRadius, int offsetSample) {
    for (int yo = offsetSample; yo >= offsetSample - foliageHeight; --yo) {
        int r = std::max(leafRadius + att.radiusOffset - 1 - yo / 2, 0);
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, r, yo, att.doubleTrunk);
    }
}

// ==========================================================================
// FancyFoliagePlacer (circular, extends BlobFoliagePlacer)
// ==========================================================================

void FancyFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                         const TreeConfig& config,
                                         int /*treeHeight*/, const FoliageAttachment& att,
                                         int foliageHeight, int leafRadius, int offsetSample) {
    for (int yo = offsetSample; yo >= offsetSample - foliageHeight; --yo) {
        // Middle rows get +1 radius (rounded canopy top and bottom excluded)
        int r = leafRadius + (yo != offsetSample && yo != offsetSample - foliageHeight ? 1 : 0);
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, r, yo, att.doubleTrunk);
    }
}

// ==========================================================================
// SpruceFoliagePlacer
// ==========================================================================

void SpruceFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                          const TreeConfig& config,
                                          int /*treeHeight*/, const FoliageAttachment& att,
                                          int foliageHeight, int leafRadius, int offsetSample) {
    int currentRadius = rng.nextInt(2);
    int maxRadius     = 1;
    int minRadius     = 0;

    for (int yo = offsetSample; yo >= -foliageHeight; --yo) {
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, currentRadius, yo, att.doubleTrunk);
        if (currentRadius >= maxRadius) {
            currentRadius = minRadius;
            minRadius     = 1;
            maxRadius     = std::min(maxRadius + 1, leafRadius + att.radiusOffset);
        } else {
            ++currentRadius;
        }
    }
}

// ==========================================================================
// PineFoliagePlacer
// ==========================================================================

void PineFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                        const TreeConfig& config,
                                        int /*treeHeight*/, const FoliageAttachment& att,
                                        int foliageHeight, int leafRadius, int offsetSample) {
    int currentRadius = 0;
    for (int yo = offsetSample; yo >= offsetSample - foliageHeight; --yo) {
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, currentRadius, yo, att.doubleTrunk);
        if (currentRadius >= 1 && yo == offsetSample - foliageHeight + 1) {
            --currentRadius;
        } else if (currentRadius < leafRadius + att.radiusOffset) {
            ++currentRadius;
        }
    }
}

// ==========================================================================
// AcaciaFoliagePlacer
// ==========================================================================

void AcaciaFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng,
                                          const TreeConfig& config,
                                          int /*treeHeight*/, const FoliageAttachment& att,
                                          int foliageHeight, int leafRadius, int offsetSample) {
    bool dt = att.doubleTrunk;
    int posY = att.y + offsetSample;
    // Acacia: 3 rows — wide, narrow, wide again — at y-1, y, and y+1 offsets
    placeLeavesRow(world, rng, config, att.x, posY, att.z,
                    leafRadius + att.radiusOffset, -1 - foliageHeight, dt);
    placeLeavesRow(world, rng, config, att.x, posY, att.z,
                    leafRadius - 1,                -foliageHeight,     dt);
    placeLeavesRow(world, rng, config, att.x, posY, att.z,
                    leafRadius + att.radiusOffset - 1, 0,              dt);
}

// ==========================================================================
// TrunkPlacer::placeLog
// ==========================================================================

bool TrunkPlacer::placeLog(TreeWorld& world, RandomSource& /*rng*/,
                             int wx, int wy, int wz,
                             const TreeConfig& config,
                             int axisOverride) {
    if (!world.validTreePos(wx, wy, wz)) return false;
    uint32_t stateId = (axisOverride < 0) ? config.logStateId :
                       (axisOverride == 0) ? config.logXStateId :
                       (axisOverride == 1) ? config.logStateId  :
                       config.logZStateId;
    world.setBlock(wx, wy, wz, stateId);
    return true;
}

// ==========================================================================
// StraightTrunkPlacer
// Port of StraightTrunkPlacer.placeTrunk
// ==========================================================================

std::vector<FoliageAttachment> StraightTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) {

    // belowTrunkProvider: place dirt one block below origin
    world.setBlock(ox, oy - 1, oz, config.dirtStateId);

    // Place straight vertical trunk
    for (int y = 0; y < treeHeight; ++y) {
        placeLog(world, rng, ox, oy + y, oz, config);
    }

    // Single attachment at top of trunk, radiusOffset=0, doubleTrunk=false
    return {{ ox, oy + treeHeight, oz, 0, false }};
}

// ==========================================================================
// ForkingTrunkPlacer
// Port of ForkingTrunkPlacer.placeTrunk (acacia)
// ==========================================================================

std::vector<FoliageAttachment> ForkingTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) {

    world.setBlock(ox, oy - 1, oz, config.dirtStateId);

    std::vector<FoliageAttachment> attachments;

    // Pick random horizontal lean direction (one of 4 cardinal directions)
    // Java: Direction.Plane.HORIZONTAL.getRandomDirection(random)
    static const int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
    int leanDir = rng.nextInt(4);
    int ldx = dirs[leanDir][0];
    int ldz = dirs[leanDir][1];

    int leanHeight  = treeHeight - rng.nextInt(4) - 1;
    int leanSteps   = 3 - rng.nextInt(3);

    int tx = ox, tz = oz;
    std::optional<int> eyY;

    for (int yo = 0; yo < treeHeight; ++yo) {
        int yy = oy + yo;
        if (yo >= leanHeight && leanSteps > 0) {
            tx += ldx;
            tz += ldz;
            --leanSteps;
        }
        if (placeLog(world, rng, tx, yy, tz, config)) {
            eyY = yy + 1;
        }
    }

    if (eyY.has_value()) {
        attachments.push_back({ tx, *eyY, tz, 1, false });
    }

    // Branch in a different horizontal direction
    tx = ox; tz = oz;
    int branchDir = rng.nextInt(4);
    if (branchDir == leanDir) branchDir = (branchDir + 1) % 4; // ensure different
    int bdx = dirs[branchDir][0];
    int bdz = dirs[branchDir][1];

    int branchStart  = leanHeight - rng.nextInt(2) - 1;
    int branchSteps2 = 1 + rng.nextInt(3);
    eyY = std::nullopt;

    for (int yo = branchStart; yo < treeHeight && branchSteps2 > 0; --branchSteps2) {
        if (yo >= 1) {
            int yy = oy + yo;
            tx += bdx;
            tz += bdz;
            if (placeLog(world, rng, tx, yy, tz, config)) {
                eyY = yy + 1;
            }
        }
        ++yo;
    }

    if (eyY.has_value()) {
        attachments.push_back({ tx, *eyY, tz, 0, false });
    }

    return attachments;
}

// ==========================================================================
// FancyTrunkPlacer
// Port of FancyTrunkPlacer.placeTrunk (large oak with branches)
// ==========================================================================

float FancyTrunkPlacer::treeShape(int height, int y) {
    if (y < (int)(height * 0.3f)) return -1.0f;
    float radius   = height / 2.0f;
    float adjacent = radius - y;
    if (adjacent == 0.0f) return radius * 0.5f;
    if (std::abs(adjacent) >= radius) return 0.0f;
    float dist = std::sqrt(radius * radius - adjacent * adjacent);
    return dist * 0.5f;
}

bool FancyTrunkPlacer::makeLimb(TreeWorld& world, RandomSource& rng,
                                  int x0, int y0, int z0,
                                  int x1, int y1, int z1,
                                  bool doPlace,
                                  const TreeConfig& config) {
    if (!doPlace && x0 == x1 && y0 == y1 && z0 == z1) return true;

    int ddx = x1 - x0, ddy = y1 - y0, ddz = z1 - z0;
    int steps = std::max({std::abs(ddx), std::abs(ddy), std::abs(ddz)});
    if (steps == 0) {
        if (doPlace) placeLog(world, rng, x0, y0, z0, config);
        return true;
    }

    float fdx = (float)ddx / steps;
    float fdy = (float)ddy / steps;
    float fdz = (float)ddz / steps;

    for (int i = 0; i <= steps; ++i) {
        int bx = x0 + (int)std::floor(0.5f + i * fdx);
        int by = y0 + (int)std::floor(0.5f + i * fdy);
        int bz = z0 + (int)std::floor(0.5f + i * fdz);

        if (doPlace) {
            // Determine axis from direction of limb
            int axisOverride = 1; // Y default
            int xd = std::abs(bx - x0), zd = std::abs(bz - z0), yd = std::abs(by - y0);
            int maxD = std::max({xd, yd, zd});
            if (maxD > 0) {
                if (xd == maxD) axisOverride = 0; // X
                else if (zd == maxD) axisOverride = 2; // Z
                else axisOverride = 1; // Y
            }
            placeLog(world, rng, bx, by, bz, config, axisOverride);
        } else {
            if (!world.isFree(bx, by, bz)) return false;
        }
    }
    return true;
}

void FancyTrunkPlacer::makeBranches(TreeWorld& world, RandomSource& rng,
                                      int height, int ox, int oy, int oz,
                                      const std::vector<FoliageCoords>& coords,
                                      const TreeConfig& config) {
    for (const auto& fc : coords) {
        int branchBase = fc.branchBase;
        if (trimBranches(height, branchBase - oy)) {
            // Only draw branch if its endpoint differs from branchBase
            if (fc.attachment.x != ox || fc.attachment.y != branchBase || fc.attachment.z != oz) {
                makeLimb(world, rng, ox, branchBase, oz,
                          fc.attachment.x, fc.attachment.y, fc.attachment.z,
                          true, config);
            }
        }
    }
}

std::vector<FoliageAttachment> FancyTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng,
        int treeHeight, int ox, int oy, int oz,
        const TreeConfig& config) {

    world.setBlock(ox, oy - 1, oz, config.dirtStateId);

    int height    = treeHeight + 2;
    int trunkH    = (int)std::floor(height * 0.618);
    // clustersPerY = min(1, floor(1.382 + (height/13)^2))
    int clustersPerY = std::min(1, (int)std::floor(1.382 + std::pow(1.0 * height / 13.0, 2.0)));
    int trunkTop  = oy + trunkH;
    int relativeY = height - 5;

    std::vector<FoliageCoords> foliageCoords;
    foliageCoords.push_back({ {ox, oy + relativeY, oz, 0, false}, trunkTop });

    for (; relativeY >= 0; --relativeY) {
        float shape = treeShape(height, relativeY);
        if (shape < 0.0f) continue;

        for (int i = 0; i < clustersPerY; ++i) {
            double radius = (double)shape * ((double)rng.nextFloat() + 0.328);
            double angle  = (double)rng.nextFloat() * 2.0 * M_PI;
            double x = radius * std::sin(angle) + 0.5;
            double z = radius * std::cos(angle) + 0.5;

            int csx = ox + (int)std::floor(x);
            int csz = oz + (int)std::floor(z);
            int csy = oy + relativeY - 1;

            // Check clearance of 6-block vertical column at (csx, csy)
            if (!makeLimb(world, rng, csx, csy, csz, csx, csy + 5, csz, false, config))
                continue;

            // Compute branch height: lower if far from trunk
            double dx = ox - csx, dz = oz - csz;
            double branchH = csy - std::sqrt(dx * dx + dz * dz) * 0.381;
            int branchTop  = (branchH > trunkTop) ? trunkTop : (int)branchH;

            // Check clearance of branch from trunk to cluster start
            if (!makeLimb(world, rng, ox, branchTop, oz, csx, csy, csz, false, config))
                continue;

            foliageCoords.push_back({ {csx, csy, csz, 0, false}, branchTop });
        }
    }

    // Place main trunk (Y axis)
    makeLimb(world, rng, ox, oy, oz, ox, oy + trunkH, oz, true, config);
    makeBranches(world, rng, height, ox, oy, oz, foliageCoords, config);

    // Build final attachment list (only include canopy clusters that pass height filter)
    std::vector<FoliageAttachment> attachments;
    for (const auto& fc : foliageCoords) {
        if (trimBranches(height, fc.branchBase - oy)) {
            attachments.push_back(fc.attachment);
        }
    }
    return attachments;
}

// ==========================================================================
// Built-in tree configs
// ==========================================================================

// ==========================================================================
// DarkOakFoliagePlacer — flat wide canopy (radii 2/3/2 around a 2×2 attachment)
// ==========================================================================

void DarkOakFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
        int /*treeHeight*/, const FoliageAttachment& att, int /*foliageHeight*/, int leafRadius, int offsetSample) {
    const bool dt = att.doubleTrunk;
    const int ox = att.x, oy = att.y + offsetSample, oz = att.z, lr = leafRadius;
    if (dt) {
        placeLeavesRow(world, rng, config, ox, oy, oz, lr + 2, -1, dt);
        placeLeavesRow(world, rng, config, ox, oy, oz, lr + 3, 0, dt);
        placeLeavesRow(world, rng, config, ox, oy, oz, lr + 2, 1, dt);
        if (rng.nextBoolean()) placeLeavesRow(world, rng, config, ox, oy, oz, lr, 2, dt);
    } else {
        placeLeavesRow(world, rng, config, ox, oy, oz, lr + 2, -1, dt);
        placeLeavesRow(world, rng, config, ox, oy, oz, lr + 1, 0, dt);
    }
}

// ==========================================================================
// MegaPineFoliagePlacer — cone crown widening downward (mega spruce / pine)
// ==========================================================================

void MegaPineFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
        int /*treeHeight*/, const FoliageAttachment& att, int foliageHeight, int leafRadius, int offsetSample) {
    const int ox = att.x, oy = att.y, oz = att.z;
    for (int k = 0; k <= foliageHeight; ++k) {
        const int yo = offsetSample - k;
        int radius = std::min(1 + k / 2, leafRadius + att.radiusOffset + 4);
        if (k == foliageHeight) radius = std::max(radius - 1, 0);
        placeLeavesRow(world, rng, config, ox, oy, oz, radius, yo, att.doubleTrunk);
    }
}

// ==========================================================================
// JungleFoliagePlacer — small rounded blob at the top of a mega jungle tree
// ==========================================================================

void JungleFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
        int /*treeHeight*/, const FoliageAttachment& att, int foliageHeight, int leafRadius, int offsetSample) {
    for (int yo = offsetSample; yo >= offsetSample - foliageHeight; --yo) {
        const bool edge = yo == offsetSample || yo == offsetSample - foliageHeight;
        const int r = std::max(leafRadius + att.radiusOffset - (edge ? 1 : 0), 0);
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, r, yo, att.doubleTrunk);
    }
}

// ==========================================================================
// DarkOakTrunkPlacer — 2×2 leaning trunk with random branches
// ==========================================================================

namespace { const int kDirs[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } }; }

std::vector<FoliageAttachment> DarkOakTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng, int treeHeight, int ox, int oy, int oz, const TreeConfig& config) {
    for (int dx = 0; dx <= 1; ++dx)
        for (int dz = 0; dz <= 1; ++dz) world.setBlock(ox + dx, oy - 1, oz + dz, config.dirtStateId);

    const int d = rng.nextInt(4);
    const int ddx = kDirs[d][0], ddz = kDirs[d][1];
    int bend = treeHeight - rng.nextInt(4);
    int steps = 2 - rng.nextInt(3);
    int cx = ox, cz = oz;
    const int topY = oy + treeHeight - 1;
    for (int yo = 0; yo < treeHeight; ++yo) {
        if (yo >= bend && steps > 0) { cx += ddx; cz += ddz; --steps; }
        const int yy = oy + yo;
        if (world.validTreePos(cx, yy, cz)) {
            placeLog(world, rng, cx, yy, cz, config);
            placeLog(world, rng, cx + 1, yy, cz, config);
            placeLog(world, rng, cx, yy, cz + 1, config);
            placeLog(world, rng, cx + 1, yy, cz + 1, config);
        }
    }
    std::vector<FoliageAttachment> atts;
    atts.push_back({ cx, topY, cz, 0, true });
    for (int bx = -1; bx <= 2; ++bx)
        for (int bz = -1; bz <= 2; ++bz) {
            if ((bx < 0 || bx > 1 || bz < 0 || bz > 1) && rng.nextInt(3) == 0) {
                const int len = rng.nextInt(3) + 2;
                for (int k = 0; k < len; ++k) placeLog(world, rng, cx + bx, topY - k - 1, cz + bz, config);
                atts.push_back({ cx + bx, topY, cz + bz, 0, false });
            }
        }
    return atts;
}

// ==========================================================================
// GiantTrunkPlacer / MegaJungleTrunkPlacer — straight 2×2 giant trunk
// ==========================================================================

void GiantTrunkPlacer::placeFourLogs(TreeWorld& world, RandomSource& rng, int wy, int ox, int oz, const TreeConfig& config) {
    placeLog(world, rng, ox, wy, oz, config);
    placeLog(world, rng, ox + 1, wy, oz, config);
    placeLog(world, rng, ox, wy, oz + 1, config);
    placeLog(world, rng, ox + 1, wy, oz + 1, config);
}

std::vector<FoliageAttachment> GiantTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng, int treeHeight, int ox, int oy, int oz, const TreeConfig& config) {
    for (int dx = 0; dx <= 1; ++dx)
        for (int dz = 0; dz <= 1; ++dz) world.setBlock(ox + dx, oy - 1, oz + dz, config.dirtStateId);
    for (int yo = 0; yo < treeHeight; ++yo) placeFourLogs(world, rng, oy + yo, ox, oz, config);
    return { { ox, oy + treeHeight, oz, 0, true } };
}

std::vector<FoliageAttachment> MegaJungleTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng, int treeHeight, int ox, int oy, int oz, const TreeConfig& config) {
    std::vector<FoliageAttachment> atts = GiantTrunkPlacer::placeTrunk(world, rng, treeHeight, ox, oy, oz, config);
    // side branches with their own foliage, in the upper half of the trunk
    for (int yo = treeHeight - 2 - rng.nextInt(4); yo > treeHeight / 2; yo -= 2 + rng.nextInt(4)) {
        const int d = rng.nextInt(4);
        const int ddx = kDirs[d][0], ddz = kDirs[d][1];
        int bx = ox, bz = oz;
        const int len = 1 + rng.nextInt(3);
        for (int k = 0; k < len; ++k) { bx += ddx; bz += ddz; placeLog(world, rng, bx, oy + yo, bz, config); }
        atts.push_back({ bx, oy + yo, bz, 0, false });
    }
    return atts;
}

// ==========================================================================
// CherryFoliagePlacer — wide rounded canopy
// ==========================================================================

void CherryFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
        int /*treeHeight*/, const FoliageAttachment& att, int foliageHeight, int leafRadius, int offsetSample) {
    const int r = leafRadius + att.radiusOffset;
    for (int yo = offsetSample + 1; yo >= offsetSample - foliageHeight; --yo) {
        const int dist = offsetSample + 1 - yo;
        const int rad = std::max((dist == 0 || dist >= foliageHeight) ? r - 2 : r, 0);
        placeLeavesRow(world, rng, config, att.x, att.y, att.z, rad, yo, att.doubleTrunk);
    }
}

// ==========================================================================
// RandomSpreadFoliagePlacer — scattered leaf cloud (mangrove)
// ==========================================================================

void RandomSpreadFoliagePlacer::createFoliage(TreeWorld& world, RandomSource& rng, const TreeConfig& config,
        int /*treeHeight*/, const FoliageAttachment& att, int foliageHeight, int leafRadius, int offsetSample) {
    const int r = leafRadius + att.radiusOffset;
    const int span = 2 * r + 1;
    for (int i = 0; i < attempts; ++i) {
        const int x = att.x + rng.nextInt(span) - r;
        const int z = att.z + rng.nextInt(span) - r;
        const int y = att.y + offsetSample + rng.nextInt(foliageHeight + 1);
        if (world.validTreePos(x, y, z)) world.setBlock(x, y, z, config.leavesStateId);
    }
}

// ==========================================================================
// CherryTrunkPlacer — vertical trunk + curving upward branches
// ==========================================================================

std::vector<FoliageAttachment> CherryTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng, int treeHeight, int ox, int oy, int oz, const TreeConfig& config) {
    world.setBlock(ox, oy - 1, oz, config.dirtStateId);
    for (int y = 0; y < treeHeight; ++y) placeLog(world, rng, ox, oy + y, oz, config);

    std::vector<FoliageAttachment> atts;
    atts.push_back({ ox, oy + treeHeight, oz, 0, false });

    const int bc = branchCount.sample(rng);
    for (int b = 0; b < bc; ++b) {
        const int d = rng.nextInt(4), ddx = kDirs[d][0], ddz = kDirs[d][1];
        const int hlen = std::max(branchHorizontalLength.sample(rng), 1);
        const int startY = treeHeight + branchStartOffset.sample(rng);
        const int endY = treeHeight + branchEndOffset.sample(rng);
        int cx = ox, cz = oz;
        for (int s = 1; s <= hlen; ++s) {
            cx += ddx; cz += ddz;
            const int yy = oy + startY + (endY - startY) * s / hlen;
            placeLog(world, rng, cx, yy, cz, config);
        }
        atts.push_back({ cx, oy + endY + 1, cz, 0, false });
    }
    return atts;
}

// ==========================================================================
// UpwardsBranchingTrunkPlacer — mangrove (vertical trunk + upward branches)
// ==========================================================================

std::vector<FoliageAttachment> UpwardsBranchingTrunkPlacer::placeTrunk(
        TreeWorld& world, RandomSource& rng, int treeHeight, int ox, int oy, int oz, const TreeConfig& config) {
    world.setBlock(ox, oy - 1, oz, config.dirtStateId);
    std::vector<FoliageAttachment> atts;
    int cx = ox, cz = oz;
    for (int yo = 0; yo < treeHeight; ++yo) {
        const int yy = oy + yo;
        placeLog(world, rng, cx, yy, cz, config);
        if (yo > 0 && yo < treeHeight - 1 && rng.nextDouble() < branchProb) {
            const int d = rng.nextInt(4), ddx = kDirs[d][0], ddz = kDirs[d][1];
            const int steps = extraBranchSteps.sample(rng);
            int bx = cx, bz = cz, by = yy;
            for (int s = 0; s < steps; ++s) { bx += ddx; bz += ddz; ++by; placeLog(world, rng, bx, by, bz, config); }
            atts.push_back({ bx, by + 1, bz, 0, false });
        }
    }
    atts.push_back({ cx, oy + treeHeight, cz, 0, false });
    return atts;
}

static uint32_t logY(const char* name) {
    // States are ordered axis=x, axis=y, axis=z  → default+1 = y-axis
    return getDefaultBlockStateId(name, 0) + 1;
}
static uint32_t logX(const char* name) {
    return getDefaultBlockStateId(name, 0);         // axis=x = default state
}
static uint32_t logZ(const char* name) {
    return getDefaultBlockStateId(name, 0) + 2;     // axis=z = default+2
}

TreeConfig makeOakConfig() {
    auto trunk   = std::make_shared<StraightTrunkPlacer>(4, 2, 0);
    auto foliage = std::make_shared<BlobFoliagePlacer>(
        IntVal::constant(2), IntVal::constant(0), 3);
    auto size    = std::make_shared<TwoLayersFeatureSize>(1, 0, 1);
    return {
        logY("oak_log"), logX("oak_log"), logZ("oak_log"),
        getDefaultBlockStateId("oak_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

TreeConfig makeBirchConfig() {
    auto trunk   = std::make_shared<StraightTrunkPlacer>(5, 2, 0);
    auto foliage = std::make_shared<BlobFoliagePlacer>(
        IntVal::constant(2), IntVal::constant(0), 3);
    auto size    = std::make_shared<TwoLayersFeatureSize>(1, 0, 1);
    return {
        logY("birch_log"), logX("birch_log"), logZ("birch_log"),
        getDefaultBlockStateId("birch_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

TreeConfig makeSpruceConfig() {
    auto trunk   = std::make_shared<StraightTrunkPlacer>(5, 2, 1);
    auto foliage = std::make_shared<SpruceFoliagePlacer>(
        IntVal::uniform(2, 3), IntVal::uniform(0, 2), IntVal::uniform(1, 2));
    auto size    = std::make_shared<TwoLayersFeatureSize>(2, 0, 2);
    return {
        logY("spruce_log"), logX("spruce_log"), logZ("spruce_log"),
        getDefaultBlockStateId("spruce_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

TreeConfig makePineConfig() {
    auto trunk   = std::make_shared<StraightTrunkPlacer>(6, 4, 0);
    auto foliage = std::make_shared<PineFoliagePlacer>(
        IntVal::constant(1), IntVal::constant(1), IntVal::uniform(3, 4));
    auto size    = std::make_shared<TwoLayersFeatureSize>(2, 0, 2);
    return {
        logY("spruce_log"), logX("spruce_log"), logZ("spruce_log"),
        getDefaultBlockStateId("spruce_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

TreeConfig makeAcaciaConfig() {
    auto trunk   = std::make_shared<ForkingTrunkPlacer>(5, 2, 2);
    auto foliage = std::make_shared<AcaciaFoliagePlacer>(
        IntVal::constant(2), IntVal::constant(0));
    auto size    = std::make_shared<TwoLayersFeatureSize>(1, 0, 2);
    return {
        logY("acacia_log"), logX("acacia_log"), logZ("acacia_log"),
        getDefaultBlockStateId("acacia_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

TreeConfig makeFancyOakConfig() {
    auto trunk   = std::make_shared<FancyTrunkPlacer>(3, 11, 0);
    auto foliage = std::make_shared<FancyFoliagePlacer>(
        IntVal::constant(2), IntVal::constant(4), 4);
    auto size    = std::make_shared<TwoLayersFeatureSize>(0, 0, 0, std::optional<int>(4));
    return {
        logY("oak_log"), logX("oak_log"), logZ("oak_log"),
        getDefaultBlockStateId("oak_leaves", 0),
        getDefaultBlockStateId("dirt", 0),
        trunk, foliage, size, true
    };
}

// ==========================================================================
// TreeFeature — placeTree
// Port of TreeFeature.doPlace + TreeFeature.place (combined, decorators skipped)
// ==========================================================================

// Port of TreeFeature.getMaxFreeTreeHeight
static int getMaxFreeTreeHeight(TreeWorld& world, int treeHeight,
                                  int ox, int oy, int oz,
                                  const TreeConfig& config) {
    for (int yo = 0; yo <= treeHeight + 1; ++yo) {
        int r = config.minimumSize->getSizeAtHeight(treeHeight, yo);
        for (int dx = -r; dx <= r; ++dx) {
            for (int dz = -r; dz <= r; ++dz) {
                if (!config.trunkPlacer->isFree(world, ox + dx, oy + yo, oz + dz)) {
                    return yo - 2;
                }
            }
        }
    }
    return treeHeight;
}

bool placeTree(TreeWorld& world, RandomSource& rng,
                int originX, int originY, int originZ,
                const TreeConfig& config) {
    int treeHeight   = config.trunkPlacer->getTreeHeight(rng);
    int foliageHeight= config.foliagePlacer->foliageHeight(rng, treeHeight, config);
    int trunkHeight  = treeHeight - foliageHeight;
    int leafRadius   = config.foliagePlacer->foliageRadius(rng, trunkHeight);

    // Height-bounds check: must fit between world floor and ceiling
    int minY = originY;
    int maxY = originY + treeHeight + 1;
    if (minY < CHUNK_MIN_Y + 1 || maxY > CHUNK_MAX_Y + 1) return false;

    // Don't originate a tree on top of another tree. The chunk heightmap rises onto
    // a neighbour's leaves (placed via cross-chunk decoration), so the OCEAN_FLOOR
    // placement can land the origin on foliage; the trunk placer would then drop a
    // dirt block onto the leaves and grow a tree on top (the "tree on a tree" / lone
    // floating-dirt artifact). Require the block below the trunk base to be ground,
    // not wood/leaves (and not air/fluid — belt-and-braces against water mushrooms).
    {
        const mc::BlockState* below = mc::getBlockState(world.getBlock(originX, originY - 1, originZ));
        if (below && below->block) {
            const std::string& bn = below->block->name;
            const bool wood = bn.find("_leaves") != std::string::npos || bn.find("_log") != std::string::npos ||
                              bn.find("_wood") != std::string::npos || bn.find("_stem") != std::string::npos ||
                              bn.find("mushroom_block") != std::string::npos;
            if (wood || below->isAir() || below->isFluid()) return false;
        } else {
            return false; // no ground below
        }
    }

    // Clearance check — may clip tree height
    int clippedHeight = getMaxFreeTreeHeight(world, treeHeight, originX, originY, originZ, config);
    auto minClipped   = config.minimumSize->minClippedHeight();
    if (clippedHeight < treeHeight) {
        if (!minClipped.has_value() || clippedHeight < *minClipped) return false;
    }

    // Place trunk and collect foliage attachment points
    std::vector<FoliageAttachment> attachments = config.trunkPlacer->placeTrunk(
        world, rng, clippedHeight, originX, originY, originZ, config);

    if (attachments.empty()) return false;

    // Place foliage at each attachment
    for (const auto& att : attachments) {
        config.foliagePlacer->createFoliage(
            world, rng, config, clippedHeight, att, foliageHeight, leafRadius);
    }

    return true;
}

// ==========================================================================
// Chunk decoration — places trees in a freshly-surfaced chunk
// ==========================================================================

// Per-chunk seeding follows the Java pattern closely enough for
// reproducible chunk-level placement (not per-position like Java feature seeds).
static uint64_t chunkSeed(uint64_t worldSeed, int cx, int cz) {
    return worldSeed
        ^ (static_cast<uint64_t>(cx) * 341873128712ULL)
        ^ (static_cast<uint64_t>(cz) * 132897987541ULL);
}

void decorateChunk(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter) {
    const ChunkPos pos   = chunk.pos();
    const int      minX  = pos.x * 16;
    const int      minZ  = pos.z * 16;

    TreeWorld world{ chunk, minX, minZ };

    const uint32_t grassId = getDefaultBlockStateId("grass_block", 0);
    const uint32_t dirtId  = getDefaultBlockStateId("dirt", 0);
    const uint32_t airId   = 0;

    // TREE TYPE SELECTION
    static TreeConfig oakCfg    = makeOakConfig();
    static TreeConfig birchCfg  = makeBirchConfig();
    static TreeConfig spruceCfg = makeSpruceConfig();
    static TreeConfig pineCfg   = makePineConfig();
    static TreeConfig acaciaCfg = makeAcaciaConfig();

    // Seeded random for this chunk
    LegacyRandomSource rng(static_cast<int64_t>(chunkSeed(worldSeed, pos.x, pos.z)));

    // Density: Java plains uses countExtra(0, 0.05, 1) ≈ 0.05 trees/chunk.
    // We use 10 attempts × 1/10 rarity filter ≈ 1 tree/chunk for visibility.
    constexpr int TREE_ATTEMPTS = 10;

    // LEAF CLIPPING MARGIN
    constexpr int TREE_MARGIN = 4; // 4 > max_leaf_radius(2), safe for oak/birch

    for (int attempt = 0; attempt < TREE_ATTEMPTS; ++attempt) {
        int lx = rng.nextInt(16);
        int lz = rng.nextInt(16);

        // Rarity filter (~10 % success)
        if (rng.nextInt(10) != 0) continue;

        // Reject positions too close to the chunk edge to avoid leaf clipping
        if (lx < TREE_MARGIN || lx >= 16 - TREE_MARGIN ||
            lz < TREE_MARGIN || lz >= 16 - TREE_MARGIN) continue;

        // Find surface: scan down from heightmap to first grass or dirt block
        int surfaceY = chunk.heightmap(lx, lz);
        while (surfaceY > CHUNK_MIN_Y) {
            uint32_t s = chunk.getBlock(minX + lx, surfaceY, minZ + lz);
            if (s == grassId || s == dirtId) break;
            --surfaceY;
        }
        if (surfaceY <= CHUNK_MIN_Y) continue;

        uint32_t ground = chunk.getBlock(minX + lx, surfaceY, minZ + lz);
        if (ground != grassId && ground != dirtId) continue;

        int originX = minX + lx;
        int originY = surfaceY + 1;
        int originZ = minZ + lz;

        // Skip if first log position is already occupied
        if (chunk.getBlock(originX, originY, originZ) != airId) continue;

        std::string biome = biomeGetter(originX, originY, originZ);
        TreeConfig* selectedCfg = &oakCfg;

        if (biome.find("taiga") != std::string::npos || 
            biome == "minecraft:grove" || 
            biome == "minecraft:snowy_slopes") {
            selectedCfg = rng.nextInt(3) == 0 ? &pineCfg : &spruceCfg;
        } else if (biome.find("savanna") != std::string::npos) {
            selectedCfg = &acaciaCfg;
        } else if (biome == "minecraft:forest" || biome == "minecraft:flower_forest" || biome == "minecraft:wooded_badlands") {
            selectedCfg = rng.nextInt(3) == 0 ? &birchCfg : &oakCfg;
        }

        placeTree(world, rng, originX, originY, originZ, *selectedCfg);
    }

    chunk.computeHeightmap();
}

} // namespace mc::levelgen::feature
