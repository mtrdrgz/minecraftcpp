// Parity gate for the FULL net.minecraft...pools.JigsawPlacement.addPieces (entry
// lines 63-159 + Placer.tryPlacingChildren 294-489) — the jigsaw structure-ASSEMBLY
// loop — vs the REAL generator (mcpp/build/jigsaw_placement.tsv, committed oracle).
// Reproduces the full placed-piece list for pillager_outpost per seed and compares
// (element location, rotation, post-move boundingBox, groundLevelDelta, junction count)
// in builder order. Heightmap is the constant-64 stub (matching the oracle GT), so this
// certifies the assembly LOGIC (RNG / alignment / collision / junctions) — the noise
// pipeline is certified separately.
//
//   inputs (run from repo root):
//     mcpp/build/structure_template_loader.tsv  (TEMPLATE rows -> base64 .nbt per template)
//     26.1.2/data/minecraft/worldgen/template_pool/pillager_outpost/*.json
//     mcpp/build/jigsaw_placement.tsv           (the committed oracle)

#include "StructureTemplatePool.h"
#include "../templatesystem/StructureTemplateLoader.h"
#include "../../../block/JigsawAttach.h"
#include "../../RandomSource.h"
#include "../../../../phys/shapes/Shapes.h"
#include "../../../../phys/AABB.h"
#include "../../../../../util/SequencedPriorityIterator.h"

#include <nlohmann/json.hpp>
#include "../../../../../nbt/NbtIo.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Vec3i;
using mc::levelgen::structure::BlockPos;
using mc::levelgen::structure::Rotation;
namespace pools = mc::levelgen::structure::pools;
namespace stl = mc::levelgen::structure::templatesystem;
namespace phys = mc;   // AABB lives in namespace mc
namespace shp = mc;    // Shapes / BooleanOps / VoxelShapePtr live in namespace mc

namespace {

std::vector<std::string> splitTabs(const std::string& s) {
    std::vector<std::string> o; std::string c; std::istringstream ss(s);
    while (std::getline(ss, c, '\t')) o.push_back(c);
    return o;
}
std::string readFile(const std::string& p) {
    std::ifstream f(p, std::ios::binary); if (!f) return {};
    std::ostringstream ss; ss << f.rdbuf(); return ss.str();
}
// standard base64 decode.
std::vector<std::uint8_t> b64decode(const std::string& in) {
    static const std::string A = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int tbl[256]; for (int i = 0; i < 256; ++i) tbl[i] = -1;
    for (int i = 0; i < 64; ++i) tbl[(unsigned char)A[i]] = i;
    std::vector<std::uint8_t> out; int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || tbl[c] == -1) continue;
        val = (val << 6) + tbl[c]; bits += 6;
        if (bits >= 0) { out.push_back((std::uint8_t)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

// Direction ordinals (standard MC): DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
int stepY(int d) { return d == 1 ? 1 : (d == 0 ? -1 : 0); }
BlockPos relative(const BlockPos& p, int d) {
    switch (d) {
        case 0: return {p.x, p.y - 1, p.z};   // DOWN
        case 1: return {p.x, p.y + 1, p.z};   // UP
        case 2: return {p.x, p.y, p.z - 1};   // NORTH
        case 3: return {p.x, p.y, p.z + 1};   // SOUTH
        case 4: return {p.x - 1, p.y, p.z};   // WEST
        case 5: return {p.x + 1, p.y, p.z};   // EAST
    }
    return p;
}
template <class D> int dirOrd(D d) { return static_cast<int>(d); }
// AABB.of(BoundingBox) — Java adds +1 to each max.
phys::AABB aabbOf(const BoundingBox& b) {
    return phys::AABB((double)b.minX, (double)b.minY, (double)b.minZ,
                      (double)(b.maxX + 1), (double)(b.maxY + 1), (double)(b.maxZ + 1));
}

// ── placed piece + free-space holder (MutableObject<VoxelShape>) ──
struct Placed {
    const pools::StructurePoolElement* element;
    std::string loc;
    BlockPos position;
    int groundLevelDelta = 0;
    Rotation rotation = Rotation::NONE;
    BoundingBox box{0, 0, 0, 0, 0, 0};
    int numJunctions = 0;
    void move(int dx, int dy, int dz) {
        box.move(dx, dy, dz);
        position = {position.x + dx, position.y + dy, position.z + dz};
    }
};
struct Free { shp::VoxelShapePtr v; };                 // v==null means unset
using FreePtr = std::shared_ptr<Free>;
struct PieceState { int pieceIdx; FreePtr free; int depth; };

constexpr int UNSET_HEIGHT = INT32_MIN;

struct Placer {
    std::map<std::string, pools::StructureTemplatePool>* poolsMap;
    std::map<std::string, stl::LoadedTemplate>* templates;
    pools::SizeResolver sizeOf;
    int maxDepth;
    bool doExpansionHack;
    std::vector<Placed>* pieces;
    std::shared_ptr<mc::levelgen::RandomSource> random;
    mc::util::SequencedPriorityIterator<PieceState> placing;

    // location for getJigsaws: single/legacy -> own; list -> elements[0].
    const stl::LoadedTemplate& templateOf(const pools::StructurePoolElement& e) {
        const std::string& loc = (e.type == pools::ElementType::LIST) ? e.elements[0].location : e.location;
        return templates->at(loc);
    }
    std::vector<stl::JigsawBlockInfo> shuffledJigsaws(const pools::StructurePoolElement& e,
                                                      const BlockPos& pos, Rotation rot) {
        // FeaturePoolElement.getShuffledJigsawBlocks (:57-70): exactly ONE synthetic
        // "bottom" jigsaw at `pos`, orientation FrontAndTop(front=DOWN, top=SOUTH) —
        // the rotation is IGNORED (no transform) and NO random draw is consumed —
        // pointing into the empty pool (name="minecraft:bottom", pool/target=empty,
        // joint=ROLLABLE). It has no template, so templateOf() must NOT be called.
        if (e.type == pools::ElementType::FEATURE) {
            stl::JigsawBlockInfo fj;
            fj.info.pos = pos;
            fj.info.blockName = "minecraft:jigsaw";
            fj.info.orientation = stl::FrontAndTop{mc::Direction::DOWN, mc::Direction::SOUTH};
            fj.info.hasOrientation = true;
            fj.jointType = stl::JointType::ROLLABLE;
            fj.name = "minecraft:bottom";
            fj.pool = "minecraft:empty";
            fj.target = "minecraft:empty";
            fj.placementPriority = 0;
            fj.selectionPriority = 0;
            return {fj};
        }
        if (e.type == pools::ElementType::EMPTY) return {};   // EmptyPoolElement: no jigsaws
        return stl::getShuffledJigsawBlocks(templateOf(e), pos, rot, *random);
    }

    void tryPlacingChildren(int sourceIdx, FreePtr contextFree, int depth) {
        const pools::StructurePoolElement& sourceElement = *(*pieces)[sourceIdx].element;
        BlockPos sourceBoxPosition = (*pieces)[sourceIdx].position;
        Rotation sourceRotation = (*pieces)[sourceIdx].rotation;
        pools::Projection sourceProjection = sourceElement.getProjection();
        bool sourceRigid = sourceProjection == pools::Projection::RIGID;
        FreePtr sourceFree = std::make_shared<Free>();
        BoundingBox sourceBB = (*pieces)[sourceIdx].box;
        int sourceBoxY = sourceBB.minY;

        for (const stl::JigsawBlockInfo& sourceJigsaw : shuffledJigsaws(sourceElement, sourceBoxPosition, sourceRotation)) {
            int sourceDirection = dirOrd(sourceJigsaw.info.orientation.front);
            BlockPos sourceJigsawPos = sourceJigsaw.info.pos;
            BlockPos targetJigsawPos = relative(sourceJigsawPos, sourceDirection);
            int sourceJigsawLocalY = sourceJigsawPos.y - sourceBoxY;
            int sourceJigsawBaseHeight = UNSET_HEIGHT;
            const std::string& poolName = sourceJigsaw.pool;
            auto pit = poolsMap->find(poolName);
            if (pit == poolsMap->end()) continue;                  // empty/non-existent pool
            pools::StructureTemplatePool& targetPool = pit->second;
            bool targetIsEmptyPool = (poolName == "minecraft:empty");
            if (targetPool.size() == 0 && !targetIsEmptyPool) continue;
            const std::string& fbName = targetPool.getFallbackName();
            auto fit = poolsMap->find(fbName);
            if (fit == poolsMap->end()) continue;
            pools::StructureTemplatePool& fallback = fit->second;
            bool fbIsEmptyPool = (fbName == "minecraft:empty");
            if (fallback.size() == 0 && !fbIsEmptyPool) { /* warn only */ }

            bool attachInsideSource = sourceBB.isInside(targetJigsawPos);
            FreePtr childrenFree;
            if (attachInsideSource) {
                childrenFree = sourceFree;
                if (!sourceFree->v) sourceFree->v = shp::Shapes::create(aabbOf(sourceBB));
            } else {
                childrenFree = contextFree;
            }

            std::vector<const pools::StructurePoolElement*> targetPieces;
            if (depth != maxDepth) {
                for (int i : targetPool.getShuffledTemplateIndices(*random))
                    targetPieces.push_back(&targetPool.templates[(std::size_t)i]);
            }
            for (int i : fallback.getShuffledTemplateIndices(*random))
                targetPieces.push_back(&fallback.templates[(std::size_t)i]);
            int placementPriority = sourceJigsaw.placementPriority;

            bool placed = false;
            for (const pools::StructurePoolElement* targetElement : targetPieces) {
                if (targetElement->isEmpty()) break;
                for (auto blockRot : mc::block::rotationGetShuffled(*random)) {
                    Rotation targetRotation = static_cast<Rotation>(static_cast<int>(blockRot));
                    std::vector<stl::JigsawBlockInfo> targetJigsaws =
                        shuffledJigsaws(*targetElement, BlockPos{0, 0, 0}, targetRotation);
                    BoundingBox hackBox = targetElement->getBoundingBox(sizeOf, BlockPos{0, 0, 0}, targetRotation);
                    int expandTo = 0;
                    if (doExpansionHack && hackBox.getYSpan() <= 16) {
                        for (const stl::JigsawBlockInfo& tj : targetJigsaws) {
                            BlockPos rp = relative(tj.info.pos, dirOrd(tj.info.orientation.front));
                            if (!hackBox.isInside(rp)) continue;
                            auto cit = poolsMap->find(tj.pool);
                            int childPoolSize = 0, childFbSize = 0;
                            if (cit != poolsMap->end()) {
                                childPoolSize = cit->second.getMaxSize(sizeOf);
                                auto cfit = poolsMap->find(cit->second.getFallbackName());
                                if (cfit != poolsMap->end()) childFbSize = cfit->second.getMaxSize(sizeOf);
                            }
                            expandTo = std::max(expandTo, std::max(childPoolSize, childFbSize));
                        }
                    }
                    for (const stl::JigsawBlockInfo& targetJigsaw : targetJigsaws) {
                        bool can = mc::block::canAttach(
                            static_cast<mc::Direction>(dirOrd(sourceJigsaw.info.orientation.front)),
                            static_cast<mc::Direction>(dirOrd(sourceJigsaw.info.orientation.top)),
                            sourceJigsaw.target,
                            static_cast<mc::block::JointType>(static_cast<int>(sourceJigsaw.jointType)),
                            static_cast<mc::Direction>(dirOrd(targetJigsaw.info.orientation.front)),
                            static_cast<mc::Direction>(dirOrd(targetJigsaw.info.orientation.top)),
                            targetJigsaw.name);
                        if (!can) continue;
                        BlockPos targetJigsawLocalPos = targetJigsaw.info.pos;
                        BlockPos rawTargetBoxPos = {targetJigsawPos.x - targetJigsawLocalPos.x,
                                                    targetJigsawPos.y - targetJigsawLocalPos.y,
                                                    targetJigsawPos.z - targetJigsawLocalPos.z};
                        BoundingBox rawTargetBB = targetElement->getBoundingBox(sizeOf, rawTargetBoxPos, targetRotation);
                        int rawTargetY = rawTargetBB.minY;
                        pools::Projection targetProjection = targetElement->getProjection();
                        bool targetRigid = targetProjection == pools::Projection::RIGID;
                        int targetJigsawLocalY = targetJigsawLocalPos.y;
                        int deltaY = sourceJigsawLocalY - targetJigsawLocalY + stepY(sourceDirection);
                        int targetBoxY;
                        if (sourceRigid && targetRigid) {
                            targetBoxY = sourceBoxY + deltaY;
                        } else {
                            if (sourceJigsawBaseHeight == UNSET_HEIGHT) sourceJigsawBaseHeight = 64; // getFirstFreeHeight stub
                            targetBoxY = sourceJigsawBaseHeight - targetJigsawLocalY;
                        }
                        int yOffset = targetBoxY - rawTargetY;
                        BoundingBox targetBB = rawTargetBB.moved(0, yOffset, 0);
                        BlockPos targetBoxPosition = {rawTargetBoxPos.x, rawTargetBoxPos.y + yOffset, rawTargetBoxPos.z};
                        if (expandTo > 0) {
                            int newSize = std::max(expandTo + 1, targetBB.maxY - targetBB.minY);
                            targetBB.encapsulate(Vec3i{targetBB.minX, targetBB.minY + newSize, targetBB.minZ});
                        }
                        if (!shp::Shapes::joinIsNotEmpty(childrenFree->v,
                                shp::Shapes::create(aabbOf(targetBB).deflate(0.25)),
                                shp::BooleanOps::ONLY_SECOND)) {
                            childrenFree->v = shp::Shapes::joinUnoptimized(
                                childrenFree->v, shp::Shapes::create(aabbOf(targetBB)), shp::BooleanOps::ONLY_FIRST);
                            int sourceGroundLevelDelta = (*pieces)[sourceIdx].groundLevelDelta;
                            int targetGroundLevelDelta = targetRigid ? (sourceGroundLevelDelta - deltaY)
                                                                     : targetElement->getGroundLevelDelta();
                            Placed tp;
                            tp.element = targetElement;
                            tp.loc = targetElement->locationString();
                            tp.position = targetBoxPosition;
                            tp.groundLevelDelta = targetGroundLevelDelta;
                            tp.rotation = targetRotation;
                            tp.box = targetBB;
                            // junctions: source gets +1, target gets +1 (count only).
                            (*pieces)[sourceIdx].numJunctions += 1;
                            tp.numJunctions += 1;
                            pieces->push_back(tp);
                            int targetIdx = (int)pieces->size() - 1;
                            if (depth + 1 <= maxDepth) placing.add(PieceState{targetIdx, childrenFree, depth + 1}, placementPriority);
                            placed = true;
                            break;
                        }
                    }
                    if (placed) break;
                }
                if (placed) break;
            }
        }
    }
};

// Per-structure addPieces arguments, read off the CONFIG row (which the GT emits from
// the REAL JigsawStructure fields — see JigsawPlacementParity.java:415-426).
struct Cfg {
    std::string startPool;
    int maxDepth = 0;
    BlockPos startPos{0, 0, 0};   // already height-projected by the GT's startHeight.sample
    bool project = false;         // projectStartToHeightmap present
    int maxDist = 0;              // MaxDistance.horizontal()==vertical() for both structures
    bool expansion = false;       // useExpansionHack
    int padBottom = 0, padTop = 0;
    bool startJigsaw = false;     // unsupported (both structures absent); asserted below
};
struct Row { std::string loc; int rot; int bb[6]; int gld; int nj; };

}  // namespace

int main(int argc, char** argv) {
    std::string oracleTsv = "mcpp/build/jigsaw_placement.tsv";
    std::string poolBase = "26.1.2/data/minecraft/worldgen/template_pool";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--pooldir" && i + 1 < argc) poolBase = argv[++i];
        else if (a == "--cases" && i + 1 < argc) oracleTsv = argv[++i];
    }
    namespace fs = std::filesystem;

    // ── single pass over the oracle: CONFIG / NBT / PIECE / COUNT, all tagged by structure ──
    std::vector<std::string> structOrder;                                    // encounter order
    std::map<std::string, Cfg> cfgs;
    std::map<std::string, std::map<std::string, stl::LoadedTemplate>> templatesByStruct;
    std::map<std::string, std::map<int64_t, std::vector<Row>>> expectedByStruct;
    {
        std::ifstream f(oracleTsv, std::ios::binary);
        if (!f) { std::fprintf(stderr, "cannot open %s\n", oracleTsv.c_str()); return 2; }
        std::string line;
        while (std::getline(f, line)) {
            auto c = splitTabs(line);
            if (c.empty()) continue;
            if (c[0] == "CONFIG") {
                // CONFIG name startPool maxDepth posX posY posZ project maxDist expansion padB padT startJigsaw
                if (c.size() < 13) continue;
                Cfg cfg;
                cfg.startPool = c[2];
                cfg.maxDepth = std::stoi(c[3]);
                cfg.startPos = {std::stoi(c[4]), std::stoi(c[5]), std::stoi(c[6])};
                cfg.project = std::stoi(c[7]) != 0;
                cfg.maxDist = std::stoi(c[8]);
                cfg.expansion = std::stoi(c[9]) != 0;
                cfg.padBottom = std::stoi(c[10]); cfg.padTop = std::stoi(c[11]);
                cfg.startJigsaw = std::stoi(c[12]) != 0;
                if (cfgs.find(c[1]) == cfgs.end()) structOrder.push_back(c[1]);
                cfgs[c[1]] = cfg;
            } else if (c[0] == "NBT") {
                // NBT name location base64(gzip .nbt)
                if (c.size() < 4) continue;
                auto bytes = b64decode(c[3]);
                auto root = mc::nbt::NbtReader::readGzip(bytes);
                if (!root) { std::fprintf(stderr, "readGzip failed for %s\n", c[2].c_str()); return 2; }
                templatesByStruct[c[1]][c[2]] = stl::loadStructureTemplate(*root);
            } else if (c[0] == "PIECE") {
                // PIECE name seed idx loc rot pX pY pZ bbMin3 bbMax3 gld nj (17 cols)
                if (c.size() < 17) continue;
                Row r; r.loc = c[4]; r.rot = std::stoi(c[5]);
                for (int k = 0; k < 6; ++k) r.bb[k] = std::stoi(c[9 + k]);
                r.gld = std::stoi(c[15]); r.nj = std::stoi(c[16]);
                expectedByStruct[c[1]][std::stoll(c[2])].push_back(r);
            } else if (c[0] == "COUNT") {
                // COUNT name seed numPieces — seed 0-count seeds so they are still tested.
                if (c.size() < 4) continue;
                int64_t seed = std::stoll(c[2]);
                expectedByStruct[c[1]].try_emplace(seed);   // empty vector if not already present
            }
        }
    }

    long checks = 0, mismatches = 0, seedsBad = 0;
    for (const std::string& sname : structOrder) {
        const Cfg& cfg = cfgs.at(sname);
        if (cfg.startJigsaw) { std::fprintf(stderr, "%s: startJigsaw unsupported — skipping\n", sname.c_str()); continue; }
        auto& templates = templatesByStruct[sname];
        auto& expected = expectedByStruct[sname];

        pools::SizeResolver sizeOf = [&templates](const std::string& loc) -> Vec3i {
            auto it = templates.find(loc);
            if (it == templates.end()) throw std::runtime_error("no template " + loc);
            return it->second.size;
        };

        // pools: every .json under template_pool/<dir> (recursive — nested dirs like
        // trail_ruins/tower/, trail_ruins/buildings/, village/plains/...), keyed
        // minecraft:<relpath-no-.json>; plus the universal empty pool. The pool subtree
        // root is the FIRST path segment of the start-pool id (after "minecraft:") — e.g.
        // village_plains -> "village", so the tag name and pool dir may differ.
        std::map<std::string, pools::StructureTemplatePool> poolsMap;
        fs::path base = fs::path(poolBase);
        std::string sp = cfg.startPool;
        if (sp.rfind("minecraft:", 0) == 0) sp = sp.substr(10);
        std::string poolSubdir = sp.substr(0, sp.find('/'));
        fs::path structDir = base / poolSubdir;
        for (auto& ent : fs::recursive_directory_iterator(structDir)) {
            if (!ent.is_regular_file() || ent.path().extension() != ".json") continue;
            std::string rel = fs::relative(ent.path(), base).generic_string();   // <struct>/.../x.json
            std::string key = "minecraft:" + rel.substr(0, rel.size() - 5);       // strip ".json"
            poolsMap[key] = pools::loadPool(nlohmann::json::parse(readFile(ent.path().string())));
        }
        poolsMap["minecraft:empty"] = pools::StructureTemplatePool{};

        long sChecks = 0, sBad = 0, sMis = 0;
        for (auto& [seed, exp] : expected) {
            ++checks; ++sChecks;
            // ── ENTRY (JigsawPlacement.addPieces, lines 63-159) ──
            auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
                std::make_shared<mc::levelgen::LegacyRandomSource>(0));
            random->setLargeFeatureSeed(seed, 0, 0);
            Rotation centerRotation = static_cast<Rotation>(random->nextInt(4));
            auto poolIt = poolsMap.find(cfg.startPool);
            if (poolIt == poolsMap.end()) { ++seedsBad; ++sBad; continue; }
            pools::StructureTemplatePool& centerPool = poolIt->second;
            int idx = centerPool.getRandomTemplateIndex(*random);
            const pools::StructurePoolElement& centerElement = centerPool.templates[(std::size_t)idx];

            std::vector<Placed> pieces;
            bool bad = false;
            try {
                // startJigsaw absent -> anchoredPosition==position==startPos, localAnchor==0.
                if (centerElement.isEmpty()) { if (!exp.empty()) { ++seedsBad; ++sBad; } continue; }
                BlockPos adjustedPosition = cfg.startPos;
                BoundingBox box = centerElement.getBoundingBox(sizeOf, adjustedPosition, centerRotation);
                Placed center;
                center.element = &centerElement; center.loc = centerElement.locationString();
                center.position = adjustedPosition; center.groundLevelDelta = 1;
                center.rotation = centerRotation; center.box = box;
                int centerX = (box.maxX + box.minX) / 2;
                int centerZ = (box.maxZ + box.minZ) / 2;
                int bottomY = cfg.project ? cfg.startPos.y + 64 : adjustedPosition.y;   // getFirstFreeHeight stub == 64
                int oldAbsoluteGroundY = box.minY + 1;
                center.move(0, bottomY - oldAbsoluteGroundY, 0);
                pieces.push_back(center);
                if (cfg.maxDepth > 0) {
                    int centerY = bottomY + 0;   // localAnchor.y == 0
                    BoundingBox cbox = pieces[0].box;
                    // heightAccessor: overworld minY=-64, maxY(incl)=319 -> maxY+1=320.
                    int loY = std::max(centerY - cfg.maxDist, -64 + cfg.padBottom);
                    int hiY = std::min(centerY + cfg.maxDist + 1, 320 - cfg.padTop);
                    phys::AABB region((double)(centerX - cfg.maxDist), (double)loY, (double)(centerZ - cfg.maxDist),
                                      (double)(centerX + cfg.maxDist + 1), (double)hiY, (double)(centerZ + cfg.maxDist + 1));
                    shp::VoxelShapePtr shape = shp::Shapes::join(shp::Shapes::create(region),
                                                                 shp::Shapes::create(aabbOf(cbox)),
                                                                 shp::BooleanOps::ONLY_FIRST);
                    Placer placer;
                    placer.poolsMap = &poolsMap; placer.templates = &templates; placer.sizeOf = sizeOf;
                    placer.maxDepth = cfg.maxDepth; placer.doExpansionHack = cfg.expansion;
                    placer.pieces = &pieces; placer.random = random;
                    auto rootFree = std::make_shared<Free>(); rootFree->v = shape;
                    placer.tryPlacingChildren(0, rootFree, 0);
                    while (auto st = placer.placing.nextOrEnd())
                        placer.tryPlacingChildren(st->pieceIdx, st->free, st->depth);
                }
            } catch (const std::exception& e) { std::fprintf(stderr, "%s seed %lld threw: %s\n", sname.c_str(), (long long)seed, e.what()); bad = true; }

            if (bad || pieces.size() != exp.size()) {
                ++seedsBad; ++sBad;
                if (sBad <= 8) std::fprintf(stderr, "%s seed=%lld got=%zu want=%zu\n", sname.c_str(), (long long)seed, pieces.size(), exp.size());
                continue;
            }
            for (std::size_t k = 0; k < pieces.size(); ++k) {
                const Placed& p = pieces[k]; const Row& r = exp[k];
                bool ok = p.loc == r.loc && (int)p.rotation == r.rot && p.groundLevelDelta == r.gld &&
                          p.numJunctions == r.nj &&
                          p.box.minX == r.bb[0] && p.box.minY == r.bb[1] && p.box.minZ == r.bb[2] &&
                          p.box.maxX == r.bb[3] && p.box.maxY == r.bb[4] && p.box.maxZ == r.bb[5];
                if (!ok) {
                    ++mismatches; ++sMis;
                    if (sMis <= 12)
                        std::fprintf(stderr, "%s seed=%lld piece=%zu got(%s r%d gld%d nj%d [%d,%d,%d..%d,%d,%d]) want(%s r%d gld%d nj%d [%d,%d,%d..%d,%d,%d])\n",
                            sname.c_str(), (long long)seed, k, p.loc.c_str(), (int)p.rotation, p.groundLevelDelta, p.numJunctions,
                            p.box.minX, p.box.minY, p.box.minZ, p.box.maxX, p.box.maxY, p.box.maxZ,
                            r.loc.c_str(), r.rot, r.gld, r.nj, r.bb[0], r.bb[1], r.bb[2], r.bb[3], r.bb[4], r.bb[5]);
                }
            }
        }
        std::printf("  %s: seeds=%ld seedsBad=%ld pieceMismatches=%ld\n", sname.c_str(), sChecks, sBad, sMis);
    }

    std::printf("JigsawPlacement structures=%zu seeds=%ld seedsBad=%ld pieceMismatches=%ld\n",
                structOrder.size(), checks, seedsBad, mismatches);
    return (seedsBad > 0 || mismatches > 0) ? 1 : 0;
}
