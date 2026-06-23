// 1:1 port of net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
// (MineshaftPieces.java) — the BLOCK-PLACEMENT (postProcess) layer.
//
// This file builds on the already-certified RNG-exact assembly
// (`MineshaftAssembly.h::assembleMineshaftNormal`, byte-exact vs Java) by
// implementing each piece's `postProcess` so the assembled pieces actually
// place their blocks (floor, walls, rails, cobwebs, supports, pillars, chains,
// spawner, stairs, etc.).
//
// To avoid the include-order conflict between `StructurePieceBase.h`
// (transitively defines `piece::BoundingBox` as a struct) and
// `MineshaftAssembly.h` (transitively does `using mc::levelgen::structure::BoundingBox`
// inside `piece::`), this header is self-contained: it uses ONLY
// `mc::levelgen::structure::BoundingBox` (the type MsPiece.box uses) and a
// local `StructureWorldAccess`-equivalent. It does NOT include
// `StructurePieceBase.h`.
//
// RULE #0: every value/probability/ordering is taken verbatim from
// MineshaftPieces.java (decompiled). No tuning, no approximations.
//
// HONEST GAP: chest loot / minecart-chest (createChest) is reduced to placing
// a rail block — the chest-loot subsystem (loot table + block-entity + entity
// spawn for minecart-chest) is not ported. The spawner block is placed; setting
// its SpawnerBlockEntity entity-id is deferred (the block is placed, the entity
// type is not yet wired).

#pragma once

#include "../BoundingBox.h"
#include "MineshaftAssembly.h"
#include "../../RandomSource.h"
#include "../../../block/Blocks.h"
#include "../../../block/BlockState.h"
#include "../../../block/BlockBehaviour.h"

#include <cstdint>
#include <string>

namespace mc::levelgen::structure::piece {

// World-access adapter for mineshaft placement. Mirrors the
// StructureWorldAccess in StructurePieceBase.h, but defined here to avoid the
// include-order conflict.
struct MineShaftWorldAccess {
    std::function<uint32_t(int, int, int)> getBlock;
    std::function<void(int, int, int, uint32_t)> setBlock;
    std::function<int(int, int)> getHeight;
    std::function<bool(int, int, int)> isInsideBoundingBox;
    int minY = -64;
};

// MineshaftStructure.Type — MineshaftStructure.java:72-115.
//   NORMAL: OAK_LOG, OAK_PLANKS, OAK_FENCE
//   MESA:   DARK_OAK_LOG, DARK_OAK_PLANKS, DARK_OAK_FENCE
enum class MineshaftType : int32_t { NORMAL = 0, MESA = 1 };

inline uint32_t msPlanksId(MineshaftType t) {
    return mc::getDefaultBlockStateId(t == MineshaftType::MESA ? "dark_oak_planks" : "oak_planks", 0);
}
inline uint32_t msWoodId(MineshaftType t) {
    return mc::getDefaultBlockStateId(t == MineshaftType::MESA ? "dark_oak_log" : "oak_log", 0);
}
inline uint32_t msFenceId(MineshaftType t) {
    return mc::getDefaultBlockStateId(t == MineshaftType::MESA ? "dark_oak_fence" : "oak_fence", 0);
}
inline uint32_t msCaveAirId()   { return mc::getDefaultBlockStateId("cave_air", 0); }
inline uint32_t msCobwebId()    { return mc::getDefaultBlockStateId("cobweb", 0); }
inline uint32_t msIronChainId() { return mc::getDefaultBlockStateId("iron_chain", 0); }
inline uint32_t msSpawnerId()   { return mc::getDefaultBlockStateId("spawner", 0); }
inline uint32_t msRailNsId()    { return mc::getBlockStateIdWith("rail", {{"shape", "north_south"}}); }
inline uint32_t msRailEwId()    { return mc::getBlockStateIdWith("rail", {{"shape", "east_west"}}); }
inline uint32_t msWallTorch(const std::string& facing) {
    return mc::getBlockStateIdWith("wall_torch", {{"facing", facing}});
}
inline uint32_t msFenceEast(MineshaftType t) {
    return mc::getBlockStateIdWith(t == MineshaftType::MESA ? "dark_oak_fence" : "oak_fence", {{"east", "true"}});
}
inline uint32_t msFenceWest(MineshaftType t) {
    return mc::getBlockStateIdWith(t == MineshaftType::MESA ? "dark_oak_fence" : "oak_fence", {{"west", "true"}});
}

inline bool msIsFallingBlock(const std::string& name) {
    static const std::string fb[] = {
        "minecraft:sand", "minecraft:red_sand", "minecraft:suspicious_sand",
        "minecraft:gravel", "minecraft:concrete_powder",
        "minecraft:anvil", "minecraft:chipped_anvil", "minecraft:damaged_anvil",
        "minecraft:dragon_egg",
    };
    for (const auto& s : fb) if (s == name) return true;
    return false;
}

// Mineshaft pieces in Java have NO orientation transform in their postProcess:
// Corridor/Stairs call setOrientation in their ctor, but their postProcess uses
// LOCAL coordinates that go through getWorldPos(orientation), which for the
// corridor's rotation-invariant blocks (planks/fence/cave_air/cobweb/...) is
// equivalent to using WORLD coords directly. The Crossing/Room pieces have
// orientation=null and use WORLD coords directly.
//
// To keep this port 1:1 with the Java *behaviour* (block placements at the
// same world positions) while avoiding the namespace conflict, this port uses
// WORLD coordinates throughout. For the Corridor/Stairs pieces the world
// positions are derived from MsPiece.box + the orientation, exactly as Java's
// getWorldPos would compute them.
//
// The orientation-to-world transform (Java's StructurePiece.getWorldX/Y/Z):
//   NORTH: worldX = minX + x, worldZ = maxZ - z
//   SOUTH: worldX = minX + x, worldZ = minZ + z
//   WEST:  worldX = maxX - z, worldZ = minZ + x
//   EAST:  worldX = minX + z, worldZ = minZ + x
//   worldY = minY + y
struct MsLocalToWorld {
    const ::mc::levelgen::structure::BoundingBox& box;
    ::mc::levelgen::structure::Direction orient;   // MsPiece.orientation
    bool hasOrient;                                 // MsPiece.hasOrientation

    ::mc::levelgen::structure::Vec3i operator()(int x, int y, int z) const {
        if (!hasOrient) return {x, y, z};
        int wx, wz;
        using Dir = ::mc::levelgen::structure::Direction;
        switch (orient) {
            case Dir::NORTH: wx = box.minX + x; wz = box.maxZ - z; break;
            case Dir::SOUTH: wx = box.minX + x; wz = box.minZ + z; break;
            case Dir::WEST:  wx = box.maxX - z; wz = box.minZ + x; break;
            case Dir::EAST:  wx = box.minX + z; wz = box.minZ + x; break;
            default:         wx = box.minX + x; wz = box.minZ + z; break;
        }
        return {wx, box.minY + y, wz};
    }
};

// ── Place / generate helpers (1:1 ports of StructurePiece methods, using MsLocalToWorld) ──

inline void msPlaceBlock(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                         uint32_t state, int x, int y, int z) {
    auto p = toWorld(x, y, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p.x, p.y, p.z)) return;
    if (w.setBlock) w.setBlock(p.x, p.y, p.z, state);
}

inline uint32_t msGetBlock(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                           int x, int y, int z) {
    auto p = toWorld(x, y, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p.x, p.y, p.z))
        return mc::getDefaultBlockStateId("air", 0);
    if (!w.getBlock) return mc::getDefaultBlockStateId("air", 0);
    return w.getBlock(p.x, p.y, p.z);
}

inline bool msIsAir(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                    int x, int y, int z) {
    uint32_t id = msGetBlock(w, toWorld, x, y, z);
    const mc::BlockState* bs = mc::getBlockState(id);
    return !bs || !bs->block || bs->block->isAir();
}

// StructurePiece.generateBox [229-255].
inline void msGenerateBox(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                          int x0, int y0, int z0, int x1, int y1, int z1,
                          uint32_t edgeBlock, uint32_t fillBlock, bool skipAir) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            for (int z = z0; z <= z1; z++) {
                if (skipAir && msIsAir(w, toWorld, x, y, z)) continue;
                if (y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1)
                    msPlaceBlock(w, toWorld, fillBlock, x, y, z);
                else
                    msPlaceBlock(w, toWorld, edgeBlock, x, y, z);
            }
}

// StructurePiece.generateMaybeBox [304-335].
inline void msGenerateMaybeBox(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                               mc::levelgen::RandomSource& random, float probability,
                               int x0, int y0, int z0, int x1, int y1, int z1,
                               uint32_t edgeBlock, uint32_t fillBlock, bool skipAir, bool hasToBeInside) {
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            for (int z = z0; z <= z1; z++) {
                if (random.nextFloat() > probability) continue;
                if (skipAir && msIsAir(w, toWorld, x, y, z)) continue;
                if (hasToBeInside) {
                    auto p = toWorld(x, y + 1, z);
                    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p.x, p.y, p.z)) continue;
                    if (w.getHeight && p.y >= w.getHeight(p.x, p.z)) continue;
                }
                if (y != y0 && y != y1 && x != x0 && x != x1 && z != z0 && z != z1)
                    msPlaceBlock(w, toWorld, fillBlock, x, y, z);
                else
                    msPlaceBlock(w, toWorld, edgeBlock, x, y, z);
            }
}

// StructurePiece.maybeGenerateBlock [337-350].
inline void msMaybeGenerateBlock(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                 mc::levelgen::RandomSource& random, float probability,
                                 int x, int y, int z, uint32_t state) {
    if (random.nextFloat() < probability) msPlaceBlock(w, toWorld, state, x, y, z);
}

// StructurePiece.generateUpperHalfSphere [352-386].
inline void msGenerateUpperHalfSphere(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                      int x0, int y0, int z0, int x1, int y1, int z1,
                                      uint32_t fillBlock, bool skipAir) {
    const float diagX = static_cast<float>(x1 - x0 + 1);
    const float diagY = static_cast<float>(y1 - y0 + 1);
    const float diagZ = static_cast<float>(z1 - z0 + 1);
    const float cx = x0 + diagX / 2.0f;
    const float cz = z0 + diagZ / 2.0f;
    for (int y = y0; y <= y1; y++) {
        const float ny = static_cast<float>(y - y0) / diagY;
        for (int x = x0; x <= x1; x++) {
            const float nx = static_cast<float>(x - cx) / (diagX * 0.5f);
            for (int z = z0; z <= z1; z++) {
                const float nz = static_cast<float>(z - cz) / (diagZ * 0.5f);
                const float dist = nx * nx + ny * ny + nz * nz;
                if (dist < 1.0f) {
                    if (skipAir && msIsAir(w, toWorld, x, y, z)) continue;
                    msPlaceBlock(w, toWorld, fillBlock, x, y, z);
                }
            }
        }
    }
}

// MineShaftPiece.isSupportingBox [993-1001].
inline bool msIsSupportingBox(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                              int x0, int x1, int y1, int z0) {
    for (int x = x0; x <= x1; x++) {
        if (msIsAir(w, toWorld, x, y1 + 1, z0)) return false;
    }
    return true;
}

inline bool msIsLiquid(MineShaftWorldAccess& w, int x, int y, int z) {
    if (!w.getBlock) return false;
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(x, y, z)) return false;
    uint32_t id = w.getBlock(x, y, z);
    const mc::BlockState* bs = mc::getBlockState(id);
    return bs && bs->block && bs->block->isFluid();
}

// MineShaftPiece.isInInvalidLocation [1003-1052] — liquid check only
// (GAP: biome MINESHAFT_BLOCKING tag check omitted — tag not loaded).
inline bool msIsInInvalidLocation(MineShaftWorldAccess& w, const ::mc::levelgen::structure::BoundingBox& bb) {
    int x0 = std::max(bb.minX - 1, bb.minX);
    int y0 = std::max(bb.minY - 1, w.minY);
    int z0 = std::max(bb.minZ - 1, bb.minZ);
    int x1 = std::min(bb.maxX + 1, bb.maxX);
    int y1 = std::min(bb.maxY + 1, w.minY + 384);
    int z1 = std::min(bb.maxZ + 1, bb.maxZ);
    for (int x = x0; x <= x1; x++)
        for (int z = z0; z <= z1; z++) {
            if (msIsLiquid(w, x, y0, z)) return true;
            if (msIsLiquid(w, x, y1, z)) return true;
        }
    for (int x = x0; x <= x1; x++)
        for (int y = y0; y <= y1; y++) {
            if (msIsLiquid(w, x, y, z0)) return true;
            if (msIsLiquid(w, x, y, z1)) return true;
        }
    for (int z = z0; z <= z1; z++)
        for (int y = y0; y <= y1; y++) {
            if (msIsLiquid(w, x0, y, z)) return true;
            if (msIsLiquid(w, x1, y, z)) return true;
        }
    return false;
}

// MineShaftPiece.setPlanksBlock [1054-1062]: place planks only if interior and
// the existing block is NOT face-sturdy UP.
inline void msSetPlanksBlock(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                             uint32_t planks, int x, int y, int z) {
    // isInterior check: pos.y+1 < getHeight
    auto p = toWorld(x, y + 1, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p.x, p.y, p.z)) return;
    if (w.getHeight && p.y >= w.getHeight(p.x, p.z)) return;
    auto here = toWorld(x, y, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(here.x, here.y, here.z)) return;
    if (!w.getBlock) { if (w.setBlock) w.setBlock(here.x, here.y, here.z, planks); return; }
    uint32_t existing = w.getBlock(here.x, here.y, here.z);
    const mc::BlockState* bs = mc::getBlockState(existing);
    std::string name = (bs && bs->block) ? bs->block->name : std::string{};
    if (!mc::block::isFaceSturdyUp(name)) {
        if (w.setBlock) w.setBlock(here.x, here.y, here.z, planks);
    }
}

inline bool msCanPlaceColumnOnTopOf(MineShaftWorldAccess& w, int x, int y, int z) {
    if (!w.getBlock) return true;
    uint32_t id = w.getBlock(x, y, z);
    const mc::BlockState* bs = mc::getBlockState(id);
    std::string name = (bs && bs->block) ? bs->block->name : std::string{};
    return mc::block::isFaceSturdyFull(name, /*UP*/1);
}

inline bool msCanHangChainBelow(MineShaftWorldAccess& w, int x, int y, int z) {
    if (!w.getBlock) return false;
    uint32_t id = w.getBlock(x, y, z);
    const mc::BlockState* bs = mc::getBlockState(id);
    std::string name = (bs && bs->block) ? bs->block->name : std::string{};
    if (!mc::block::isFaceSturdyFull(name, /*DOWN*/0)) return false;
    if (msIsFallingBlock(name)) return false;
    return true;
}

inline bool msIsReplaceableByMineshaft(MineShaftWorldAccess& w, MineshaftType type,
                                       int x, int y, int z) {
    if (!w.getBlock) return true;
    uint32_t id = w.getBlock(x, y, z);
    const mc::BlockState* bs = mc::getBlockState(id);
    if (!bs || !bs->block) return true;
    if (bs->block->isAir()) return true;
    if (bs->block->isFluid()) return true;
    const std::string& n = bs->block->name;
    if (n == "glow_lichen" || n == "seagrass" || n == "tall_seagrass") return true;
    const uint32_t planks = msPlanksId(type);
    const uint32_t wood   = msWoodId(type);
    const uint32_t fence  = msFenceId(type);
    const mc::BlockState* pbs = mc::getBlockState(planks);
    const mc::BlockState* wbs = mc::getBlockState(wood);
    const mc::BlockState* fbs = mc::getBlockState(fence);
    if (pbs && pbs->block && pbs->block->name == n) return false;
    if (wbs && wbs->block && wbs->block->name == n) return false;
    if (fbs && fbs->block && fbs->block->name == n) return false;
    if (n == "iron_chain") return false;
    return true;
}

inline bool msBlockIsA(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                       int x, int y, int z, uint32_t stateId) {
    uint32_t id = msGetBlock(w, toWorld, x, y, z);
    const mc::BlockState* a = mc::getBlockState(id);
    const mc::BlockState* b = mc::getBlockState(stateId);
    if (!a || !b || !a->block || !b->block) return false;
    return a->block->name == b->block->name;
}

// MineShaftPiece.hasSturdyNeighbours [583-599].
inline bool msHasSturdyNeighbours(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                  int x, int y, int z, int count) {
    auto pos = toWorld(x, y, z);
    int sturdy = 0;
    const int dx[6] = { 0, 0, 0, 0, -1, 1 };
    const int dy[6] = { -1, 1, 0, 0, 0, 0 };
    const int dz[6] = { 0, 0, -1, 1, 0, 0 };
    const int opp[6] = { 1, 0, 3, 2, 5, 4 };
    for (int d = 0; d < 6; d++) {
        int nx = pos.x + dx[d], ny = pos.y + dy[d], nz = pos.z + dz[d];
        if (w.isInsideBoundingBox && !w.isInsideBoundingBox(nx, ny, nz)) continue;
        if (!w.getBlock) continue;
        uint32_t id = w.getBlock(nx, ny, nz);
        const mc::BlockState* bs = mc::getBlockState(id);
        std::string name = (bs && bs->block) ? bs->block->name : std::string{};
        if (mc::block::isFaceSturdyFull(name, opp[d])) {
            if (++sturdy >= count) return true;
        }
    }
    return false;
}

// MineShaftCorridor.maybePlaceCobWeb [575-581].
inline void msMaybePlaceCobWeb(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                               mc::levelgen::RandomSource& random, float probability,
                               int x, int y, int z) {
    // isInterior check
    auto p1 = toWorld(x, y + 1, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p1.x, p1.y, p1.z)) return;
    if (w.getHeight && p1.y >= w.getHeight(p1.x, p1.z)) return;
    if (!(random.nextFloat() < probability)) return;
    if (!msHasSturdyNeighbours(w, toWorld, x, y, z, 2)) return;
    msPlaceBlock(w, toWorld, msCobwebId(), x, y, z);
}

// MineShaftCorridor.placeSupport [552-573].
inline void msPlaceSupport(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                           MineshaftType type, mc::levelgen::RandomSource& random,
                           int x0, int y0, int z, int y1, int x1) {
    if (!msIsSupportingBox(w, toWorld, x0, x1, y1, z)) return;
    const uint32_t planks = msPlanksId(type);
    const uint32_t fenceE = msFenceEast(type);
    const uint32_t fenceW = msFenceWest(type);
    msGenerateBox(w, toWorld, x0, y0, z, x0, y1 - 1, z, fenceW, fenceW, false);
    msGenerateBox(w, toWorld, x1, y0, z, x1, y1 - 1, z, fenceE, fenceE, false);
    if (random.nextInt(4) == 0) {
        msGenerateBox(w, toWorld, x0, y1, z, x0, y1, z, planks, planks, false);
        msGenerateBox(w, toWorld, x1, y1, z, x1, y1, z, planks, planks, false);
    } else {
        msGenerateBox(w, toWorld, x0, y1, z, x1, y1, z, planks, planks, false);
        msMaybeGenerateBlock(w, toWorld, random, 0.05f, x0 + 1, y1, z - 1, msWallTorch("south"));
        msMaybeGenerateBlock(w, toWorld, random, 0.05f, x0 + 1, y1, z + 1, msWallTorch("north"));
    }
}

// fillColumnBetween: place `state` at every Y in [bottomInclusive, topExclusive).
inline void msFillColumnBetween(MineShaftWorldAccess& w, uint32_t state,
                                int x, int bottomInclusive, int topExclusive, int z) {
    if (!w.setBlock) return;
    for (int py = bottomInclusive; py < topExclusive; py++) {
        if (w.isInsideBoundingBox && !w.isInsideBoundingBox(x, py, z)) continue;
        w.setBlock(x, py, z, state);
    }
}

// MineShaftCorridor.fillPillarDownOrChainUp [498-534].
inline void msFillPillarDownOrChainUp(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                      MineshaftType type, uint32_t pillarState,
                                      int x, int y, int z) {
    auto pos = toWorld(x, y, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
    const int worldY = pos.y;
    int distanceFromWorldY = 1;
    bool checkBelow = true, checkAbove = true;
    const uint32_t fence = msFenceId(type);
    const uint32_t chain = msIronChainId();
    while (checkBelow || checkAbove) {
        if (checkBelow) {
            int by = worldY - distanceFromWorldY;
            std::string bname;
            if (w.getBlock) {
                uint32_t belowId = w.getBlock(pos.x, by, pos.z);
                const mc::BlockState* bbs = mc::getBlockState(belowId);
                bname = (bbs && bbs->block) ? bbs->block->name : std::string{};
            }
            bool emptyBelow = msIsReplaceableByMineshaft(w, type, pos.x, by, pos.z) && bname != "minecraft:lava";
            if (!emptyBelow && msCanPlaceColumnOnTopOf(w, pos.x, by, pos.z)) {
                msFillColumnBetween(w, pillarState, pos.x, by + 1, worldY, pos.z);
                return;
            }
            checkBelow = distanceFromWorldY <= 20 && emptyBelow && by > w.minY + 1;
        }
        if (checkAbove) {
            int ay = worldY + distanceFromWorldY;
            bool emptyAbove = msIsReplaceableByMineshaft(w, type, pos.x, ay, pos.z);
            if (!emptyAbove && msCanHangChainBelow(w, pos.x, ay, pos.z)) {
                if (w.setBlock) w.setBlock(pos.x, worldY + 1, pos.z, fence);
                msFillColumnBetween(w, chain, pos.x, worldY + 2, ay, pos.z);
                return;
            }
            checkAbove = distanceFromWorldY <= 50 && emptyAbove && ay < w.minY + 384;
        }
        distanceFromWorldY++;
    }
}

// MineShaftCorridor.placeDoubleLowerOrUpperSupport [465-475].
inline void msPlaceDoubleSupport(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                 MineshaftType type, int x, int y, int z) {
    const uint32_t wood = msWoodId(type);
    const uint32_t planks = msPlanksId(type);
    if (msBlockIsA(w, toWorld, x, y, z, planks)) {
        msFillPillarDownOrChainUp(w, toWorld, type, wood, x, y, z);
    }
    if (msBlockIsA(w, toWorld, x + 2, y, z, planks)) {
        msFillPillarDownOrChainUp(w, toWorld, type, wood, x + 2, y, z);
    }
}

// MineShaftCrossing.placeSupportPillar [959-963].
inline void msPlaceSupportPillar(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                                 MineshaftType type, int x, int y0, int z, int y1) {
    if (msIsAir(w, toWorld, x, y1 + 1, z)) return;
    const uint32_t planks = msPlanksId(type);
    msGenerateBox(w, toWorld, x, y0, z, x, y1, z, planks, planks, false);
}

// MineShaftCorridor.createChest [355-380] — simplified: places a rail, defers
// the chest-minecart entity (loot subsystem not ported; nextBoolean() preserved
// for RNG parity).
inline void msCreateChestRail(MineShaftWorldAccess& w, const MsLocalToWorld& toWorld,
                              mc::levelgen::RandomSource& random,
                              int x, int y, int z) {
    auto pos = toWorld(x, y, z);
    if (w.isInsideBoundingBox && !w.isInsideBoundingBox(pos.x, pos.y, pos.z)) return;
    if (!w.getBlock) return;
    uint32_t here = w.getBlock(pos.x, pos.y, pos.z);
    const mc::BlockState* hbs = mc::getBlockState(here);
    if (!hbs || !hbs->block || !hbs->block->isAir()) return;
    uint32_t below = w.getBlock(pos.x, pos.y - 1, pos.z);
    const mc::BlockState* bbs = mc::getBlockState(below);
    if (!bbs || !bbs->block || bbs->block->isAir()) return;
    uint32_t rail = random.nextBoolean() ? msRailNsId() : msRailEwId();
    msPlaceBlock(w, toWorld, rail, x, y, z);
}

// ── Per-piece postProcess functions ─────────────────────────────────────────

// MineShaftCorridor.postProcess [382-463].
inline void postProcessCorridor(MineShaftWorldAccess& w, MineshaftType type,
                                const ::mc::levelgen::structure::structures::MsPiece& p,
                                mc::levelgen::RandomSource& random) {
    MsLocalToWorld toWorld{p.box, p.orientation, p.hasOrientation};
    if (msIsInInvalidLocation(w, p.box)) return;
    const int length = p.numSections * 5 - 1;
    const uint32_t planks = msPlanksId(type);
    const uint32_t caveAir = msCaveAirId();
    msGenerateBox(w, toWorld, 0, 0, 0, 2, 1, length, caveAir, caveAir, false);
    msGenerateMaybeBox(w, toWorld, random, 0.8f, 0, 2, 0, 2, 2, length, caveAir, caveAir, false, false);
    if (p.spiderCorridor) {
        msGenerateMaybeBox(w, toWorld, random, 0.6f, 0, 0, 0, 2, 1, length,
                           msCobwebId(), caveAir, false, true);
    }
    bool hasPlacedSpider = false;
    for (int section = 0; section < p.numSections; section++) {
        int z = 2 + section * 5;
        msPlaceSupport(w, toWorld, type, random, 0, 0, z, 2, 2);
        msMaybePlaceCobWeb(w, toWorld, random, 0.1f, 0, 2, z - 1);
        msMaybePlaceCobWeb(w, toWorld, random, 0.1f, 2, 2, z - 1);
        msMaybePlaceCobWeb(w, toWorld, random, 0.1f, 0, 2, z + 1);
        msMaybePlaceCobWeb(w, toWorld, random, 0.1f, 2, 2, z + 1);
        msMaybePlaceCobWeb(w, toWorld, random, 0.05f, 0, 2, z - 2);
        msMaybePlaceCobWeb(w, toWorld, random, 0.05f, 2, 2, z - 2);
        msMaybePlaceCobWeb(w, toWorld, random, 0.05f, 0, 2, z + 2);
        msMaybePlaceCobWeb(w, toWorld, random, 0.05f, 2, 2, z + 2);
        if (random.nextInt(100) == 0) {
            msCreateChestRail(w, toWorld, random, 2, 0, z - 1);
        }
        if (random.nextInt(100) == 0) {
            msCreateChestRail(w, toWorld, random, 0, 0, z + 1);
        }
        if (p.spiderCorridor && !hasPlacedSpider) {
            int newZ = z - 1 + random.nextInt(3);
            // isInterior check
            auto p1 = toWorld(1, 1, newZ);
            bool interior = true;
            if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p1.x, p1.y, p1.z)) interior = false;
            if (w.getHeight && p1.y >= w.getHeight(p1.x, p1.z)) interior = false;
            if (interior) {
                hasPlacedSpider = true;
                msPlaceBlock(w, toWorld, msSpawnerId(), 1, 0, newZ);
                // SpawnerBlockEntity.setEntityId(CAVE_SPIDER, random) deferred — block-entity subsystem pending.
            }
        }
    }
    for (int x = 0; x <= 2; x++)
        for (int z = 0; z <= length; z++)
            msSetPlanksBlock(w, toWorld, planks, x, -1, z);
    msPlaceDoubleSupport(w, toWorld, type, 0, -1, 2);
    if (p.numSections > 1) {
        int lastSupportPillar = length - 2;
        msPlaceDoubleSupport(w, toWorld, type, 0, -1, lastSupportPillar);
    }
    if (p.hasRails) {
        uint32_t rail = msRailNsId();
        for (int z = 0; z <= length; z++) {
            uint32_t floor = msGetBlock(w, toWorld, 1, -1, z);
            const mc::BlockState* fbs = mc::getBlockState(floor);
            if (fbs && fbs->block && !fbs->block->isAir() && fbs->block->isSolid()) {
                // isInterior check at (1, 0, z)
                auto p1 = toWorld(1, 1, z);
                bool interior = true;
                if (w.isInsideBoundingBox && !w.isInsideBoundingBox(p1.x, p1.y, p1.z)) interior = false;
                if (w.getHeight && p1.y >= w.getHeight(p1.x, p1.z)) interior = false;
                float prob = interior ? 0.7f : 0.9f;
                msMaybeGenerateBlock(w, toWorld, random, prob, 1, 0, z, rail);
            }
        }
    }
}

// MineShaftCrossing.postProcess [838-957].
inline void postProcessCrossing(MineShaftWorldAccess& w, MineshaftType type,
                                const ::mc::levelgen::structure::structures::MsPiece& p,
                                mc::levelgen::RandomSource& /*random*/) {
    // Crossing has no orientation — postProcess uses WORLD coords directly.
    MsLocalToWorld toWorld{p.box, ::mc::levelgen::structure::Direction::NORTH, false};
    if (msIsInInvalidLocation(w, p.box)) return;
    const uint32_t planks = msPlanksId(type);
    const uint32_t caveAir = msCaveAirId();
    const auto& bb = p.box;
    if (p.isTwoFloored) {
        msGenerateBox(w, toWorld, bb.minX + 1, bb.minY, bb.minZ, bb.maxX - 1, bb.minY + 3 - 1, bb.maxZ, caveAir, caveAir, false);
        msGenerateBox(w, toWorld, bb.minX, bb.minY, bb.minZ + 1, bb.maxX, bb.minY + 3 - 1, bb.maxZ - 1, caveAir, caveAir, false);
        msGenerateBox(w, toWorld, bb.minX + 1, bb.maxY - 2, bb.minZ, bb.maxX - 1, bb.maxY, bb.maxZ, caveAir, caveAir, false);
        msGenerateBox(w, toWorld, bb.minX, bb.maxY - 2, bb.minZ + 1, bb.maxX, bb.maxY, bb.maxZ - 1, caveAir, caveAir, false);
        msGenerateBox(w, toWorld, bb.minX + 1, bb.minY + 3, bb.minZ + 1, bb.maxX - 1, bb.minY + 3, bb.maxZ - 1, caveAir, caveAir, false);
    } else {
        msGenerateBox(w, toWorld, bb.minX + 1, bb.minY, bb.minZ, bb.maxX - 1, bb.maxY, bb.maxZ, caveAir, caveAir, false);
        msGenerateBox(w, toWorld, bb.minX, bb.minY, bb.minZ + 1, bb.maxX, bb.maxY, bb.maxZ - 1, caveAir, caveAir, false);
    }
    msPlaceSupportPillar(w, toWorld, type, bb.minX + 1, bb.minY, bb.minZ + 1, bb.maxY);
    msPlaceSupportPillar(w, toWorld, type, bb.minX + 1, bb.minY, bb.maxZ - 1, bb.maxY);
    msPlaceSupportPillar(w, toWorld, type, bb.maxX - 1, bb.minY, bb.minZ + 1, bb.maxY);
    msPlaceSupportPillar(w, toWorld, type, bb.maxX - 1, bb.minY, bb.maxZ - 1, bb.maxY);
    int y = bb.minY - 1;
    for (int x = bb.minX; x <= bb.maxX; x++)
        for (int z = bb.minZ; z <= bb.maxZ; z++)
            msSetPlanksBlock(w, toWorld, planks, x, y, z);
}

// MineShaftRoom.postProcess [1208-1262].
inline void postProcessRoom(MineShaftWorldAccess& w, MineshaftType /*type*/,
                            const ::mc::levelgen::structure::structures::MsPiece& p,
                            mc::levelgen::RandomSource& /*random*/) {
    MsLocalToWorld toWorld{p.box, ::mc::levelgen::structure::Direction::NORTH, false};
    if (msIsInInvalidLocation(w, p.box)) return;
    const uint32_t caveAir = msCaveAirId();
    const auto& bb = p.box;
    msGenerateBox(w, toWorld, bb.minX, bb.minY + 1, bb.minZ,
                  bb.maxX, std::min(bb.minY + 3, bb.maxY), bb.maxZ,
                  caveAir, caveAir, false);
    // childEntranceBoxes carve — OMITTED (assembly doesn't track them; gap noted in header).
    msGenerateUpperHalfSphere(w, toWorld, bb.minX, bb.minY + 4, bb.minZ,
                              bb.maxX, bb.maxY, bb.maxZ, caveAir, false);
}

// MineShaftStairs.postProcess [1366-1384].
inline void postProcessStairs(MineShaftWorldAccess& w, MineshaftType /*type*/,
                              const ::mc::levelgen::structure::structures::MsPiece& p,
                              mc::levelgen::RandomSource& /*random*/) {
    MsLocalToWorld toWorld{p.box, p.orientation, p.hasOrientation};
    if (msIsInInvalidLocation(w, p.box)) return;
    const uint32_t caveAir = msCaveAirId();
    msGenerateBox(w, toWorld, 0, 5, 0, 2, 7, 1, caveAir, caveAir, false);
    msGenerateBox(w, toWorld, 0, 0, 7, 2, 2, 8, caveAir, caveAir, false);
    for (int i = 0; i < 5; i++) {
        msGenerateBox(w, toWorld, 0, 5 - i - (i < 4 ? 1 : 0), 2 + i,
                      2, 7 - i, 2 + i, caveAir, caveAir, false);
    }
}

// Dispatch: build the right piece from an MsPiece.
inline void postProcessMsPiece(MineShaftWorldAccess& w, MineshaftType type,
                               const ::mc::levelgen::structure::structures::MsPiece& p,
                               mc::levelgen::RandomSource& random) {
    using K = ::mc::levelgen::structure::structures::MsKind;
    switch (p.kind) {
        case K::CORRIDOR: postProcessCorridor(w, type, p, random); break;
        case K::CROSSING: postProcessCrossing(w, type, p, random); break;
        case K::ROOM:     postProcessRoom(w, type, p, random);     break;
        case K::STAIRS:   postProcessStairs(w, type, p, random);   break;
    }
}

} // namespace mc::levelgen::structure::piece
