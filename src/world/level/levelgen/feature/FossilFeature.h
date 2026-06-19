#pragma once

// 1:1 port of net.minecraft.world.level.levelgen.feature.FossilFeature over a
// minimal StructureTemplate pipeline (26.1.2 sources):
//   StructureTemplate.load (StructureTemplate.java:~700: "palette"/"palettes" +
//     "blocks" lists), with loadPalette's addToLists/buildInfoList ordering
//     (full-collision blocks first, then others, then block entities — each
//     sorted by y,x,z; StructureTemplate.java:141-173). The fossil templates
//     contain only bone_block / coal_ore full cubes.
//   StructurePlaceSettings.getRandomPalette (StructurePlaceSettings.java:138-145):
//     ONE nextInt(paletteCount) from the settings random (the FEATURE random —
//     FossilFeature sets .setRandom(random)), even for a single palette.
//   processBlockInfos (StructureTemplate.java:438-470): per block info the
//     rotation transform (StructureTemplate.transform, :~560) then the processor
//     chain; surviving infos placed in order with setBlock(flags 260) and the
//     final updateShapeAtEdge over the placed shape (placeInWorld,
//     StructureTemplate.java:251-360).
//   Processors: BlockRotProcessor (BlockRotProcessor.java:41-53 — ONE nextFloat
//     from settings.getRandom(pos) == the feature random per info; keep iff
//     nextFloat <= integrity); ProtectedBlockProcessor (ProtectedBlockProcessor
//     .java:23-33 — drop when the level block is in #features_cannot_replace);
//     RuleProcessor (RuleProcessor.java:23-44) with block_match/always_true rules
//     only (no draws; the rule random is POSITIONAL and unused by these rules).
//
// FossilFeature.place (FossilFeature.java:26-72): Rotation.getRandom (nextInt(4)),
// fossilIndex nextInt(8), the OCEAN_FLOOR_WG footprint scan, targetY =
// max(lowest - 15 - nextInt(10), minY + 10), the empty-corner gate
// (countEmptyCorners, :74-83), then base+overlay placeInWorld.

#include "../placement/PlacementContext.h"
#include "../placement/PlacedFeature.h"
#include "../RandomSource.h"
#include "../Heightmap.h"
#include "TreeFeature.h"                 // TreeVoxelShape + treeRelative (updateShapeAtEdge)
#include "../../../../core/Math.h"
#include "../../../../nbt/NbtIo.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace mc::levelgen::feature {

using mc::BlockPos;
using mc::levelgen::RandomSource;
using mc::levelgen::placement::WorldGenLevel;

struct FossilHooks {
    std::function<bool(const std::string&)> isAir;
    std::function<bool(const std::string&)> featuresCannotReplace;   // #features_cannot_replace
    // StructureTemplate.updateShapeAtEdge single face visit (the tree machinery hook).
    std::function<void(BlockPos, int, BlockPos)> updateShapeFace;
    int levelMinY = 0;
    int levelMaxY = 0;   // inclusive (level.getMaxY())
    std::string structureDir;   // .../data/minecraft/structure
};

namespace fossil_detail {

struct BlockInfo {
    BlockPos pos;          // template-relative
    std::string state;     // block id (AXIS etc. are id-invisible)
};

struct Template {
    BlockPos size{};
    std::vector<BlockInfo> blocks;   // buildInfoList order
    int paletteCount = 1;            // "palettes" list size, else 1
};

// StructureTemplate.load + loadPalette + buildInfoList. The fossil templates hold
// only full-cube blocks without nbt, so buildInfoList reduces to one sort by
// (y, x, z) (StructureTemplate.java:156-173; stable, but keys are unique cells).
inline Template loadTemplate(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("cannot open structure " + path);
    std::vector<std::uint8_t> raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto rootOpt = mc::nbt::NbtReader::readGzip(raw);
    if (!rootOpt.has_value()) throw std::runtime_error("cannot parse structure " + path);
    const mc::nbt::NbtCompound& root = *rootOpt;

    Template t;
    const mc::nbt::NbtList* sizeList = root.getList("size");
    if (!sizeList || sizeList->elements.size() != 3) throw std::runtime_error("structure size missing: " + path);
    auto asInt = [](const mc::nbt::NbtTag& tag) { return std::get<std::int32_t>(tag.value); };
    t.size = BlockPos{ asInt(sizeList->elements[0]), asInt(sizeList->elements[1]), asInt(sizeList->elements[2]) };

    // palette (single) or palettes (random palette list).
    std::vector<std::vector<std::string>> palettes;
    if (const mc::nbt::NbtList* pl = root.getList("palettes")) {
        for (const auto& palTag : pl->elements) {
            const auto& pal = *std::get<std::shared_ptr<mc::nbt::NbtList>>(palTag.value);
            std::vector<std::string> names;
            for (const auto& e : pal.elements)
                names.push_back(std::get<std::shared_ptr<mc::nbt::NbtCompound>>(e.value)->getString("Name"));
            palettes.push_back(std::move(names));
        }
    } else if (const mc::nbt::NbtList* pal = root.getList("palette")) {
        std::vector<std::string> names;
        for (const auto& e : pal->elements)
            names.push_back(std::get<std::shared_ptr<mc::nbt::NbtCompound>>(e.value)->getString("Name"));
        palettes.push_back(std::move(names));
    } else {
        throw std::runtime_error("structure palette missing: " + path);
    }
    t.paletteCount = static_cast<int>(palettes.size());
    // All fossil palettes map the same block ids per index across rotated palettes;
    // the block LIST indexes into whichever palette is selected — ids must agree
    // (fail closed otherwise, since the id-level grid cannot distinguish).
    for (std::size_t p = 1; p < palettes.size(); ++p) {
        if (palettes[p].size() != palettes[0].size()) throw std::runtime_error("palette size mismatch: " + path);
        for (std::size_t i = 0; i < palettes[0].size(); ++i)
            if (palettes[p][i] != palettes[0][i])
                throw std::runtime_error("palette BLOCK-ID mismatch across palettes (need real palette selection): " + path);
    }

    const mc::nbt::NbtList* blocks = root.getList("blocks");
    if (!blocks) throw std::runtime_error("structure blocks missing: " + path);
    for (const auto& e : blocks->elements) {
        const auto& b = *std::get<std::shared_ptr<mc::nbt::NbtCompound>>(e.value);
        const mc::nbt::NbtList* posList = b.getList("pos");
        const int stateIdx = b.getInt("state");
        if (b.getCompound("nbt") != nullptr)
            throw std::runtime_error("structure block entity not supported (fossils carry none): " + path);
        t.blocks.push_back(BlockInfo{
            BlockPos{ asInt(posList->elements[0]), asInt(posList->elements[1]), asInt(posList->elements[2]) },
            palettes[0][static_cast<std::size_t>(stateIdx)] });
    }
    // buildInfoList: every fossil entry is a full-collision cube without nbt ->
    // one stable sort by (y, x, z).
    std::stable_sort(t.blocks.begin(), t.blocks.end(), [](const BlockInfo& a, const BlockInfo& b) {
        if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
        if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        return a.pos.z < b.pos.z;
    });
    return t;
}

inline Template& templateFor(const FossilHooks& hooks, const std::string& id) {
    static std::map<std::string, Template> cache;
    auto it = cache.find(id);
    if (it != cache.end()) return it->second;
    // id "minecraft:fossil/spine_1" -> <structureDir>/fossil/spine_1.nbt
    const std::string path = hooks.structureDir + "/" +
        (id.rfind("minecraft:", 0) == 0 ? id.substr(10) : id) + ".nbt";
    return cache.emplace(id, loadTemplate(path)).first->second;
}

// Rotation indices: 0=NONE, 1=CLOCKWISE_90, 2=CLOCKWISE_180, 3=COUNTERCLOCKWISE_90
// (Rotation.java enum order; Rotation.getRandom = Util.getRandom(values(), random)).
// StructureTemplate.transform (StructureTemplate.java:~560), mirror NONE, pivot 0.
inline BlockPos transform(BlockPos pos, int rotation) {
    switch (rotation) {
        case 3: return BlockPos{ pos.z, pos.y, -pos.x };          // COUNTERCLOCKWISE_90: (pivot 0)
        case 1: return BlockPos{ -pos.z, pos.y, pos.x };          // CLOCKWISE_90
        case 2: return BlockPos{ -pos.x, pos.y, -pos.z };         // CLOCKWISE_180
        default: return pos;                                       // NONE (no mirror)
    }
}

// StructureTemplate.getZeroPositionWithTransform (static; mirror NONE).
inline BlockPos zeroPositionWithTransform(BlockPos zeroPos, int rotation, int sizeX, int sizeZ) {
    --sizeX;
    --sizeZ;
    switch (rotation) {
        case 3: return BlockPos{ zeroPos.x + 0, zeroPos.y, zeroPos.z + sizeX };   // CCW_90: offset(0, 0, sizeX)
        case 1: return BlockPos{ zeroPos.x + sizeZ, zeroPos.y, zeroPos.z + 0 };   // CW_90: offset(sizeZ, 0, 0)
        case 2: return BlockPos{ zeroPos.x + sizeX, zeroPos.y, zeroPos.z + sizeZ };
        default: return zeroPos;
    }
}

// StructureTemplate.getSize(rotation).
inline BlockPos sizeFor(const Template& t, int rotation) {
    if (rotation == 1 || rotation == 3) return BlockPos{ t.size.z, t.size.y, t.size.x };
    return t.size;
}

// BoundingBox over the transformed template at `position`
// (StructureTemplate.getBoundingBox: transform both corners, min/max, move).
struct Box { int minX, minY, minZ, maxX, maxY, maxZ; };
inline Box boundingBoxFor(const Template& t, int rotation, BlockPos position) {
    const BlockPos a = transform(BlockPos{ 0, 0, 0 }, rotation);
    const BlockPos b = transform(BlockPos{ t.size.x - 1, t.size.y - 1, t.size.z - 1 }, rotation);
    Box box{ std::min(a.x, b.x) + position.x, std::min(a.y, b.y) + position.y, std::min(a.z, b.z) + position.z,
             std::max(a.x, b.x) + position.x, std::max(a.y, b.y) + position.y, std::max(a.z, b.z) + position.z };
    return box;
}

// One processor in a list. Kind: 0 block_rot, 1 protected_blocks, 2 rule(block_match).
struct Processor {
    int kind = 0;
    float integrity = 1.0f;                       // block_rot
    struct Rule { std::string input; std::string output; };
    std::vector<Rule> rules;                      // rule processor (block_match -> output)
};

// processBlockInfos + placeInWorld block path (fossil-reachable subset).
inline bool placeTemplateInWorld(WorldGenLevel& level, const FossilHooks& hooks, RandomSource& random,
                                 const Template& t, int rotation, BlockPos position, const Box& clipBox,
                                 const std::vector<Processor>& processors) {
    if (t.blocks.empty()) return false;   // palettes empty equivalent
    // settings.getRandomPalette: nextInt(paletteCount) from the FEATURE random —
    // a real draw even with one palette (StructurePlaceSettings.java:138-145).
    (void)random.nextInt(t.paletteCount);
    struct Placed { BlockPos pos; std::string state; };
    std::vector<Placed> survivors;
    for (const BlockInfo& info : t.blocks) {
        // calculateRelativePosition = transform(pos, NONE, rotation, pivot 0) + position.
        const BlockPos worldPos = [&] {
            const BlockPos r = transform(info.pos, rotation);
            return BlockPos{ r.x + position.x, r.y + position.y, r.z + position.z };
        }();
        std::optional<std::string> state = info.state;
        for (const Processor& proc : processors) {
            if (!state.has_value()) break;
            if (proc.kind == 0) {
                // BlockRotProcessor: settings random == the feature random.
                if (!(random.nextFloat() <= proc.integrity)) state.reset();
            } else if (proc.kind == 1) {
                if (hooks.featuresCannotReplace(level.getBlockState(worldPos))) state.reset();
            } else {
                for (const Processor::Rule& rule : proc.rules) {
                    if (*state == rule.input) { state = rule.output; break; }
                }
            }
        }
        if (state.has_value()) survivors.push_back(Placed{ worldPos, *state });
    }
    int minX = INT32_MAX, minY = INT32_MAX, minZ = INT32_MAX;
    int maxX = INT32_MIN, maxY = INT32_MIN, maxZ = INT32_MIN;
    std::vector<BlockPos> placed;
    for (const Placed& p : survivors) {
        if (p.pos.x < clipBox.minX || p.pos.x > clipBox.maxX || p.pos.y < clipBox.minY || p.pos.y > clipBox.maxY
            || p.pos.z < clipBox.minZ || p.pos.z > clipBox.maxZ) {
            continue;
        }
        // keepLiquids path: bone_block/ores are not LiquidBlockContainers and their
        // states carry no fluid — the waterlogging branches are no-ops here.
        if (level.setBlockChecked(p.pos, p.state, 260)) {
            minX = std::min(minX, p.pos.x); minY = std::min(minY, p.pos.y); minZ = std::min(minZ, p.pos.z);
            maxX = std::max(maxX, p.pos.x); maxY = std::max(maxY, p.pos.y); maxZ = std::max(maxZ, p.pos.z);
            placed.push_back(p.pos);
        }
    }
    if (minX <= maxX) {
        // getKnownShape false -> updateShapeAtEdge over the placed shape
        // (StructureTemplate.java:351-365), reusing the tree voxel machinery.
        TreeVoxelShape shape(maxX - minX + 1, maxY - minY + 1, maxZ - minZ + 1);
        for (const BlockPos& p : placed) shape.fill(p.x - minX, p.y - minY, p.z - minZ);
        shape.forAllFaces([&](int dir, int x, int y, int z) {
            const BlockPos pos{ minX + x, minY + y, minZ + z };
            hooks.updateShapeFace(pos, dir, treeRelative(pos, dir));
        });
    }
    return true;
}

} // namespace fossil_detail

struct FossilConfig {
    std::vector<std::string> fossilStructures;    // identifiers
    std::vector<std::string> overlayStructures;
    std::vector<fossil_detail::Processor> fossilProcessors;
    std::vector<fossil_detail::Processor> overlayProcessors;
    int maxEmptyCornersAllowed = 4;
};

// FossilFeature.place (FossilFeature.java:26-72).
inline mc::levelgen::placement::PlacedFeature::FeaturePlacer makeFossilPlacer(
        std::shared_ptr<const FossilConfig> config, std::shared_ptr<const FossilHooks> hooks) {
    return [config = std::move(config), hooks = std::move(hooks)](
               WorldGenLevel& level, RandomSource& random, BlockPos origin) -> bool {
        using namespace fossil_detail;
        const int rotation = random.nextInt(4);   // Rotation.getRandom = values()[nextInt(4)]
        const int fossilIndex = random.nextInt(static_cast<int>(config->fossilStructures.size()));
        Template& fossilBase = templateFor(*hooks, config->fossilStructures[static_cast<std::size_t>(fossilIndex)]);
        Template& fossilOverlay = templateFor(*hooks, config->overlayStructures[static_cast<std::size_t>(fossilIndex)]);
        const int chunkX = origin.x >> 4, chunkZ = origin.z >> 4;
        const Box clipBox{ chunkX * 16 - 16, hooks->levelMinY, chunkZ * 16 - 16,
                           chunkX * 16 + 15 + 16, hooks->levelMaxY, chunkZ * 16 + 15 + 16 };
        const BlockPos size = sizeFor(fossilBase, rotation);
        const BlockPos lowCorner{ origin.x - size.x / 2, origin.y, origin.z - size.z / 2 };
        int lowestSurfaceY = origin.y;
        for (int xscan = 0; xscan < size.x; ++xscan) {
            for (int zscan = 0; zscan < size.z; ++zscan) {
                lowestSurfaceY = std::min(lowestSurfaceY,
                    level.getHeight(Heightmap::Types::OCEAN_FLOOR_WG, lowCorner.x + xscan, lowCorner.z + zscan));
            }
        }
        const int targetY = std::max(lowestSurfaceY - 15 - random.nextInt(10), hooks->levelMinY + 10);
        const BlockPos targetPos = zeroPositionWithTransform(
            BlockPos{ lowCorner.x, targetY, lowCorner.z }, rotation, fossilBase.size.x, fossilBase.size.z);
        // countEmptyCorners (FossilFeature.java:74-83) over the structure bbox.
        const Box bb = boundingBoxFor(fossilBase, rotation, targetPos);
        int emptyCorners = 0;
        for (int cx : { bb.minX, bb.maxX })
            for (int cy : { bb.minY, bb.maxY })
                for (int cz : { bb.minZ, bb.maxZ }) {
                    const std::string s = level.getBlockState(BlockPos{ cx, cy, cz });
                    if (hooks->isAir(s) || s == "minecraft:lava" || s == "minecraft:water") ++emptyCorners;
                }
        if (emptyCorners > config->maxEmptyCornersAllowed) return false;
        placeTemplateInWorld(level, *hooks, random, fossilBase, rotation, targetPos, clipBox, config->fossilProcessors);
        placeTemplateInWorld(level, *hooks, random, fossilOverlay, rotation, targetPos, clipBox, config->overlayProcessors);
        return true;
    };
}

} // namespace mc::levelgen::feature
