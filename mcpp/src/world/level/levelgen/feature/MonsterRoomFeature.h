#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.MonsterRoomFeature
// (MonsterRoomFeature.java). Writes cave_air walls/interior, cobblestone /
// mossy_cobblestone shell, up to 2 chests (loot table seeding consumes one
// nextLong each when the chest block entity exists) and one spawner (mob pick
// consumes one nextInt(4) when the spawner block entity exists).
//
// RNG order:
//   xr = nextInt(2)+2, zr = nextInt(2)+2
//   hole scan: none
//   wall pass (x asc, y 3..-1 desc, z asc): nextInt(4) for every boundary cell
//     replaced at dy == -1 (mossy vs cobble; the draw happens before the branch
//     picks, MonsterRoomFeature.java:79-83)
//   chests: 2 passes x up to 3 tries: nextInt(xr*2+1), nextInt(zr*2+1) per try;
//     on a placed chest RandomizableContainer.setBlockEntityLootTable consumes
//     random.nextLong() IF the block entity exists (the write landed) —
//     RandomizableContainer.java:42-49
//   spawner: randomEntityId = Util.getRandom(MOBS, random) = nextInt(4), evaluated
//     as the setEntityId ARGUMENT only when getBlockEntity(origin) is a spawner;
//     BaseSpawner.getOrCreateNextSpawnData draws nothing (spawnPotentials empty ->
//     WeightedList.getRandom returns empty WITHOUT consuming, WeightedList.java:66-73)
//
// safeSetBlock = Feature.safeSetBlock (Feature.java:177-181): write with flags 2
// iff !state.is(BlockTags.FEATURES_CANNOT_REPLACE). StructurePiece.reorient only
// alters the chest FACING property — id-invisible, no RNG — skipped.

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"

#include <functional>
#include <string>
#include <utility>

namespace mc::levelgen::feature {

struct MonsterRoomHooks {
    std::function<bool(const std::string&)> isSolid;               // BlockStateBase.isSolid (legacySolid)
    std::function<bool(const std::string&)> isAir;                 // isEmptyBlock
    std::function<bool(const std::string&)> featuresCannotReplace; // #minecraft:features_cannot_replace
    int levelMinY = 0;
};

inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeMonsterRoomPlacer(
        std::shared_ptr<const MonsterRoomHooks> hooks) {
    return [hooks = std::move(hooks)](WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        auto safeSetBlock = [&](BlockPos pos, const std::string& state) -> bool {
            if (!hooks->featuresCannotReplace(level.getBlockState(pos))) {
                return level.setBlockChecked(pos, state, 2);
            }
            return false;
        };
        const int xr = random.nextInt(2) + 2;
        const int minX = -xr - 1, maxX = xr + 1;
        const int zr = random.nextInt(2) + 2;
        const int minZ = -zr - 1, maxZ = zr + 1;
        int holeCount = 0;
        for (int dx = minX; dx <= maxX; ++dx) {
            for (int dy = -1; dy <= 4; ++dy) {
                for (int dz = minZ; dz <= maxZ; ++dz) {
                    const BlockPos holePos{ origin.x + dx, origin.y + dy, origin.z + dz };
                    const std::string holeState = level.getBlockState(holePos);
                    const bool solid = hooks->isSolid(holeState);
                    if (dy == -1 && !solid) return false;
                    if (dy == 4 && !solid) return false;
                    if ((dx == minX || dx == maxX || dz == minZ || dz == maxZ) && dy == 0
                        && hooks->isAir(level.getBlockState(holePos))
                        && hooks->isAir(level.getBlockState(BlockPos{ holePos.x, holePos.y + 1, holePos.z }))) {
                        ++holeCount;
                    }
                }
            }
        }
        if (holeCount < 1 || holeCount > 5) {
            return false;
        }
        // Wall/interior pass (MonsterRoomFeature.java:72-99): x asc, y 3..-1 desc, z asc.
        for (int dx = minX; dx <= maxX; ++dx) {
            for (int dy = 3; dy >= -1; --dy) {
                for (int dz = minZ; dz <= maxZ; ++dz) {
                    const BlockPos wallBlock{ origin.x + dx, origin.y + dy, origin.z + dz };
                    const std::string wallState = level.getBlockState(wallBlock);
                    if (dx == minX || dy == -1 || dz == minZ || dx == maxX || dy == 4 || dz == maxZ) {
                        if (wallBlock.y >= hooks->levelMinY
                            && !hooks->isSolid(level.getBlockState(BlockPos{ wallBlock.x, wallBlock.y - 1, wallBlock.z }))) {
                            level.setBlock(wallBlock, "minecraft:cave_air", 2);   // AIR = CAVE_AIR (MonsterRoomFeature.java:26)
                        } else if (hooks->isSolid(wallState) && wallState != "minecraft:chest") {
                            if (dy == -1 && random.nextInt(4) != 0) {
                                safeSetBlock(wallBlock, "minecraft:mossy_cobblestone");
                            } else {
                                safeSetBlock(wallBlock, "minecraft:cobblestone");
                            }
                        }
                    } else if (wallState != "minecraft:chest" && wallState != "minecraft:spawner") {
                        safeSetBlock(wallBlock, "minecraft:cave_air");
                    }
                }
            }
        }
        // Chests (MonsterRoomFeature.java:101-127).
        for (int cc = 0; cc < 2; ++cc) {
            for (int i = 0; i < 3; ++i) {
                const int xc = origin.x + random.nextInt(xr * 2 + 1) - xr;
                const int yc = origin.y;
                const int zc = origin.z + random.nextInt(zr * 2 + 1) - zr;
                const BlockPos chestPos{ xc, yc, zc };
                if (hooks->isAir(level.getBlockState(chestPos))) {
                    int wallCount = 0;
                    // Direction.Plane.HORIZONTAL: NORTH, EAST, SOUTH, WEST.
                    static constexpr int hdx[4] = { 0, 1, 0, -1 };
                    static constexpr int hdz[4] = { -1, 0, 1, 0 };
                    for (int d = 0; d < 4; ++d) {
                        if (hooks->isSolid(level.getBlockState(BlockPos{ chestPos.x + hdx[d], chestPos.y, chestPos.z + hdz[d] }))) {
                            ++wallCount;
                        }
                    }
                    if (wallCount == 1) {
                        // StructurePiece.reorient picks the FACING — id-invisible.
                        const bool placed = safeSetBlock(chestPos, "minecraft:chest");
                        // setBlockEntityLootTable: nextLong iff the block entity exists
                        // (the DUMMY nbt is only recorded for a landed write).
                        if (placed) {
                            (void)random.nextLong();
                        }
                        break;
                    }
                }
            }
        }
        safeSetBlock(origin, "minecraft:spawner");
        // if (level.getBlockEntity(origin) instanceof SpawnerBlockEntity spawner):
        // the entity exists iff a spawner sits at origin (every grid spawner was
        // feature-written through the proxy, so the DUMMY nbt is present).
        if (level.getBlockState(origin) == "minecraft:spawner") {
            (void)random.nextInt(4);   // Util.getRandom(MOBS, random); setEntityId draws nothing more
        }
        return true;
    };
}

} // namespace mc::levelgen::feature
