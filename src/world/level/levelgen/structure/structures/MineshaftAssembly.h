#pragma once
// 1:1 port of the FULL recursive mineshaft assembly:
//   net.minecraft.world.level.levelgen.structure.structures.MineshaftStructure
//       .generatePiecesAndAdjust  [MineshaftStructure.java:45-65]
//   net.minecraft.world.level.levelgen.structure.structures.MineshaftPieces
//       .createRandomShaftPiece / .generateAndAddPiece                  [48-105]
//       .MineShaftCorridor.addChildren                                  [170-353]
//       .MineShaftCrossing.addChildren                                  [649-836]
//       .MineShaftRoom.addChildren                                      [1083-1206]
//       .MineShaftStairs.addChildren                                    [1308-1364]
//
// This ties together the already-certified, per-box RNG helpers
//   * MineShaftCorridorParity.h  findCorridorSize  (nextInt(3) length loop)
//   * MineShaftCrossingBox.h     findCrossing      (nextInt(4) height pick)
//   * MineShaftRoomBox.h         makeRoomBox       (3 nextInt(6) draws)
//   * MineshaftStairsBox.h       findStairs        (no draw)
// into the depth-first recursion, reproducing the EXACT RandomSource draw order
// so the assembled piece list (kinds, boxes, orientations, corridor rails/spider/
// numSections, crossing two-floored/direction, post-sea-level offset) is byte-
// exact vs the real game. Verified by mineshaft_assembly_parity against
// tools/MineshaftAssemblyParity.java (the real MineshaftPieces driven directly).
//
// Block placement (postProcess) is NOT here — it reads back generated terrain and
// is verified separately against a fully-generated world (run_server_gen_structures.sh
// + ServerChunkDump). This header is the RNG-exact skeleton that placement builds on.
//
// NOTE: only the NORMAL vertical adjust (moveBelowSeaLevel) is reproduced; the MESA
// branch needs a ChunkGenerator (getBaseHeight) for its offset — the pre-adjust
// assembly (rooms/corridors/...) is identical, only the final dy differs.

#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "MineShaftCorridorParity.h"
#include "MineShaftCrossingBox.h"
#include "MineShaftRoomBox.h"
#include "MineshaftStairsBox.h"
#include "world/level/levelgen/structure/BoundingBox.h"
#include "world/level/levelgen/structure/StructurePieceCollection.h"
#include "world/level/levelgen/RandomSource.h"

namespace mc::levelgen::structure::structures {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::Direction;

enum class MsKind : int32_t { ROOM = 0, CORRIDOR = 1, CROSSING = 2, STAIRS = 3 };

// One assembled mineshaft piece (geometry + the flags the block placement reads).
struct MsPiece {
    MsKind     kind{};
    BoundingBox box{};
    bool        hasOrientation = false;     // CORRIDOR/STAIRS set it; ROOM/CROSSING leave it null
    Direction   orientation = Direction::NORTH;
    int32_t     genDepth = 0;
    // CORRIDOR
    bool    hasRails = false;
    bool    spiderCorridor = false;
    int32_t numSections = 0;
    // CROSSING
    bool      isTwoFloored = false;
    Direction crossingDir = Direction::NORTH;
};

namespace detail {

// MineshaftStructure.Type — assembly only needs NORMAL vs MESA for the final adjust.
enum class MsType { NORMAL, MESA };

struct MsBuilder {
    std::vector<MsPiece> pieces;

    // StructurePieceAccessor.findCollisionPiece(box) != null
    bool collides(const BoundingBox& box) const {
        std::vector<BoundingBox> boxes;
        boxes.reserve(pieces.size());
        for (const MsPiece& p : pieces) boxes.push_back(p.box);
        return mc::levelgen::structure::piece::findCollisionPieceIndex(boxes, box) >= 0;
    }
    BoundingBox aggregateBox() const {
        std::vector<BoundingBox> boxes;
        boxes.reserve(pieces.size());
        for (const MsPiece& p : pieces) boxes.push_back(p.box);
        return mc::levelgen::structure::piece::createBoundingBox(boxes);
    }
};

inline bool isZAxis(Direction d) { return d == Direction::NORTH || d == Direction::SOUTH; }

// MineshaftPieces.createRandomShaftPiece [48-77].
inline std::optional<MsPiece> createRandomShaftPiece(
        MsBuilder& b, mc::levelgen::RandomSource& r,
        int32_t footX, int32_t footY, int32_t footZ, Direction dir, int32_t genDepth) {
    int32_t sel = r.nextInt(100);
    auto collides = [&](const BoundingBox& bb) { return b.collides(bb); };
    if (sel >= 80) {
        auto box = findCrossing(r, footX, footY, footZ, dir, collides);
        if (box) {
            MsPiece p;
            p.kind = MsKind::CROSSING;
            p.box = *box;
            p.genDepth = genDepth;
            p.hasOrientation = false;                 // MineShaftCrossing has no setOrientation
            p.crossingDir = dir;
            p.isTwoFloored = box->getYSpan() > 3;
            return p;
        }
    } else if (sel >= 70) {
        // findStairs draws nothing; build the box (collisionPresent=false always
        // returns it) then test collision ourselves (Java findStairs order).
        BoundingBox box = *findStairs(false, footX, footY, footZ, dir);
        if (!b.collides(box)) {
            MsPiece p;
            p.kind = MsKind::STAIRS;
            p.box = box;
            p.genDepth = genDepth;
            p.hasOrientation = true;
            p.orientation = dir;
            return p;
        }
    } else {
        auto box = findCorridorSize(r, footX, footY, footZ, dir, collides);
        if (box) {
            MsPiece p;
            p.kind = MsKind::CORRIDOR;
            p.box = *box;
            p.genDepth = genDepth;
            p.hasOrientation = true;
            p.orientation = dir;
            // MineShaftCorridor ctor [130-142]: setOrientation, then the two draws,
            // then numSections from the box span along the orientation axis.
            p.hasRails = r.nextInt(3) == 0;
            p.spiderCorridor = !p.hasRails && r.nextInt(23) == 0;
            p.numSections = isZAxis(dir) ? box->getZSpan() / 5 : box->getXSpan() / 5;
            return p;
        }
    }
    return std::nullopt;
}

// forward
inline void addChildren(MsBuilder& b, int32_t selfIdx, mc::levelgen::RandomSource& r);

// MineshaftPieces.generateAndAddPiece [79-105]. Returns the new piece index, or -1.
inline int32_t generateAndAddPiece(
        MsBuilder& b, mc::levelgen::RandomSource& r,
        int32_t footX, int32_t footY, int32_t footZ, Direction dir, int32_t depth) {
    if (depth > 8) return -1;
    const int32_t startMinX = b.pieces[0].box.minX;   // startPiece is always the room (index 0)
    const int32_t startMinZ = b.pieces[0].box.minZ;
    if (std::abs(footX - startMinX) <= 80 && std::abs(footZ - startMinZ) <= 80) {
        auto np = createRandomShaftPiece(b, r, footX, footY, footZ, dir, depth + 1);
        if (np) {
            int32_t idx = static_cast<int32_t>(b.pieces.size());
            b.pieces.push_back(*np);
            addChildren(b, idx, r);   // newPiece.addChildren(startPiece, accessor, random)
            return idx;
        }
        return -1;
    }
    return -1;
}

// MineShaftRoom.addChildren [1083-1206]. childEntranceBoxes are recorded in Java
// for the room's own postProcess but draw no RNG and don't affect the piece list,
// so they are omitted from the assembly.
inline void roomAddChildren(MsBuilder& b, const MsPiece& self, mc::levelgen::RandomSource& r) {
    const int32_t depth = self.genDepth;
    int32_t heightSpace = self.box.getYSpan() - 3 - 1;
    if (heightSpace <= 0) heightSpace = 1;
    const BoundingBox& bb = self.box;

    // NORTH wall (iterate over XSpan)
    for (int32_t pos = 0; pos < bb.getXSpan();) {
        pos += r.nextInt(bb.getXSpan());
        if (pos + 3 > bb.getXSpan()) break;
        int32_t fy = bb.minY + r.nextInt(heightSpace) + 1;
        generateAndAddPiece(b, r, bb.minX + pos, fy, bb.minZ - 1, Direction::NORTH, depth);
        pos += 4;
    }
    // SOUTH wall (iterate over XSpan)
    for (int32_t pos = 0; pos < bb.getXSpan();) {
        pos += r.nextInt(bb.getXSpan());
        if (pos + 3 > bb.getXSpan()) break;
        int32_t fy = bb.minY + r.nextInt(heightSpace) + 1;
        generateAndAddPiece(b, r, bb.minX + pos, fy, bb.maxZ + 1, Direction::SOUTH, depth);
        pos += 4;
    }
    // WEST wall (iterate over ZSpan)
    for (int32_t pos = 0; pos < bb.getZSpan();) {
        pos += r.nextInt(bb.getZSpan());
        if (pos + 3 > bb.getZSpan()) break;
        int32_t fy = bb.minY + r.nextInt(heightSpace) + 1;
        generateAndAddPiece(b, r, bb.minX - 1, fy, bb.minZ + pos, Direction::WEST, depth);
        pos += 4;
    }
    // EAST wall (iterate over ZSpan)
    for (int32_t pos = 0; pos < bb.getZSpan();) {
        pos += r.nextInt(bb.getZSpan());
        if (pos + 3 > bb.getZSpan()) break;
        int32_t fy = bb.minY + r.nextInt(heightSpace) + 1;
        generateAndAddPiece(b, r, bb.maxX + 1, fy, bb.minZ + pos, Direction::EAST, depth);
        pos += 4;
    }
}

// MineShaftCorridor.addChildren [170-353].
inline void corridorAddChildren(MsBuilder& b, const MsPiece& self, mc::levelgen::RandomSource& r) {
    const int32_t depth = self.genDepth;
    const int32_t endSelection = r.nextInt(4);
    const Direction o = self.orientation;
    const BoundingBox& bb = self.box;
    auto y0 = [&]() { return bb.minY - 1 + r.nextInt(3); };  // draws nextInt(3) per call

    switch (o) {
        case Direction::SOUTH:
            if (endSelection <= 1)      generateAndAddPiece(b, r, bb.minX,     y0(), bb.maxZ + 1, Direction::SOUTH, depth);
            else if (endSelection == 2) generateAndAddPiece(b, r, bb.minX - 1, y0(), bb.maxZ - 3, Direction::WEST,  depth);
            else                        generateAndAddPiece(b, r, bb.maxX + 1, y0(), bb.maxZ - 3, Direction::EAST,  depth);
            break;
        case Direction::WEST:
            if (endSelection <= 1)      generateAndAddPiece(b, r, bb.minX - 1, y0(), bb.minZ,     Direction::WEST,  depth);
            else if (endSelection == 2) generateAndAddPiece(b, r, bb.minX,     y0(), bb.minZ - 1, Direction::NORTH, depth);
            else                        generateAndAddPiece(b, r, bb.minX,     y0(), bb.maxZ + 1, Direction::SOUTH, depth);
            break;
        case Direction::EAST:
            if (endSelection <= 1)      generateAndAddPiece(b, r, bb.maxX + 1, y0(), bb.minZ,     Direction::EAST,  depth);
            else if (endSelection == 2) generateAndAddPiece(b, r, bb.maxX - 3, y0(), bb.minZ - 1, Direction::NORTH, depth);
            else                        generateAndAddPiece(b, r, bb.maxX - 3, y0(), bb.maxZ + 1, Direction::SOUTH, depth);
            break;
        case Direction::NORTH:
        default:
            if (endSelection <= 1)      generateAndAddPiece(b, r, bb.minX,     y0(), bb.minZ - 1, Direction::NORTH, depth);
            else if (endSelection == 2) generateAndAddPiece(b, r, bb.minX - 1, y0(), bb.minZ,     Direction::WEST,  depth);
            else                        generateAndAddPiece(b, r, bb.maxX + 1, y0(), bb.minZ,     Direction::EAST,  depth);
            break;
    }

    if (depth < 8) {
        if (o != Direction::NORTH && o != Direction::SOUTH) {
            for (int32_t x = bb.minX + 3; x + 3 <= bb.maxX; x += 5) {
                int32_t selection = r.nextInt(5);
                if (selection == 0)      generateAndAddPiece(b, r, x, bb.minY, bb.minZ - 1, Direction::NORTH, depth + 1);
                else if (selection == 1) generateAndAddPiece(b, r, x, bb.minY, bb.maxZ + 1, Direction::SOUTH, depth + 1);
            }
        } else {
            for (int32_t z = bb.minZ + 3; z + 3 <= bb.maxZ; z += 5) {
                int32_t selection = r.nextInt(5);
                if (selection == 0)      generateAndAddPiece(b, r, bb.minX - 1, bb.minY, z, Direction::WEST, depth + 1);
                else if (selection == 1) generateAndAddPiece(b, r, bb.maxX + 1, bb.minY, z, Direction::EAST, depth + 1);
            }
        }
    }
}

// MineShaftCrossing.addChildren [649-836].
inline void crossingAddChildren(MsBuilder& b, const MsPiece& self, mc::levelgen::RandomSource& r) {
    const int32_t depth = self.genDepth;
    const BoundingBox& bb = self.box;
    switch (self.crossingDir) {
        case Direction::SOUTH:
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction::SOUTH, depth);
            generateAndAddPiece(b, r, bb.minX - 1, bb.minY, bb.minZ + 1, Direction::WEST,  depth);
            generateAndAddPiece(b, r, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction::EAST,  depth);
            break;
        case Direction::WEST:
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.minZ - 1, Direction::NORTH, depth);
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction::SOUTH, depth);
            generateAndAddPiece(b, r, bb.minX - 1, bb.minY, bb.minZ + 1, Direction::WEST,  depth);
            break;
        case Direction::EAST:
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.minZ - 1, Direction::NORTH, depth);
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.maxZ + 1, Direction::SOUTH, depth);
            generateAndAddPiece(b, r, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction::EAST,  depth);
            break;
        case Direction::NORTH:
        default:
            generateAndAddPiece(b, r, bb.minX + 1, bb.minY, bb.minZ - 1, Direction::NORTH, depth);
            generateAndAddPiece(b, r, bb.minX - 1, bb.minY, bb.minZ + 1, Direction::WEST,  depth);
            generateAndAddPiece(b, r, bb.maxX + 1, bb.minY, bb.minZ + 1, Direction::EAST,  depth);
            break;
    }
    if (self.isTwoFloored) {
        if (r.nextBoolean()) generateAndAddPiece(b, r, bb.minX + 1, bb.minY + 3 + 1, bb.minZ - 1, Direction::NORTH, depth);
        if (r.nextBoolean()) generateAndAddPiece(b, r, bb.minX - 1, bb.minY + 3 + 1, bb.minZ + 1, Direction::WEST,  depth);
        if (r.nextBoolean()) generateAndAddPiece(b, r, bb.maxX + 1, bb.minY + 3 + 1, bb.minZ + 1, Direction::EAST,  depth);
        if (r.nextBoolean()) generateAndAddPiece(b, r, bb.minX + 1, bb.minY + 3 + 1, bb.maxZ + 1, Direction::SOUTH, depth);
    }
}

// MineShaftStairs.addChildren [1308-1364].
inline void stairsAddChildren(MsBuilder& b, const MsPiece& self, mc::levelgen::RandomSource& r) {
    const int32_t depth = self.genDepth;
    const BoundingBox& bb = self.box;
    switch (self.orientation) {
        case Direction::SOUTH: generateAndAddPiece(b, r, bb.minX,     bb.minY, bb.maxZ + 1, Direction::SOUTH, depth); break;
        case Direction::WEST:  generateAndAddPiece(b, r, bb.minX - 1, bb.minY, bb.minZ,     Direction::WEST,  depth); break;
        case Direction::EAST:  generateAndAddPiece(b, r, bb.maxX + 1, bb.minY, bb.minZ,     Direction::EAST,  depth); break;
        case Direction::NORTH:
        default:               generateAndAddPiece(b, r, bb.minX,     bb.minY, bb.minZ - 1, Direction::NORTH, depth); break;
    }
}

inline void addChildren(MsBuilder& b, int32_t selfIdx, mc::levelgen::RandomSource& r) {
    // Copy the piece's fields up front: the recursion pushes more pieces and may
    // reallocate the vector. The piece's own box/flags don't change during its
    // addChildren (children are separate pieces), so the copy is faithful.
    const MsPiece self = b.pieces[selfIdx];
    switch (self.kind) {
        case MsKind::ROOM:     roomAddChildren(b, self, r); break;
        case MsKind::CORRIDOR: corridorAddChildren(b, self, r); break;
        case MsKind::CROSSING: crossingAddChildren(b, self, r); break;
        case MsKind::STAIRS:   stairsAddChildren(b, self, r); break;
    }
}

// StructurePiecesBuilder.moveBelowSeaLevel [31-43] (NORMAL adjust).
inline int32_t moveBelowSeaLevel(MsBuilder& b, int32_t seaLevel, int32_t minY,
                                 mc::levelgen::RandomSource& r, int32_t offset) {
    int32_t maxY = seaLevel - offset;
    BoundingBox agg = b.aggregateBox();
    int32_t y1Pos = agg.getYSpan() + minY + 1;
    if (y1Pos < maxY) {
        y1Pos += r.nextInt(maxY - y1Pos);
    }
    int32_t dy = y1Pos - agg.maxY;
    for (MsPiece& p : b.pieces) p.box.move(0, dy, 0);
    return dy;
}

} // namespace detail

// MineshaftStructure.findGenerationPoint + generatePiecesAndAdjust (NORMAL).
// Returns the assembled, sea-level-adjusted piece list for the start chunk.
// random: WorldgenRandom over a LegacyRandomSource(0), seeded by
// setLargeFeatureSeed(seed, chunkX, chunkZ) — exactly the structure context RNG.
inline std::vector<MsPiece> assembleMineshaftNormal(int64_t seed, int32_t chunkX, int32_t chunkZ,
                                                    int32_t* yOffsetOut = nullptr) {
    auto base = std::make_shared<mc::levelgen::LegacyRandomSource>(0);
    mc::levelgen::WorldgenRandom random(base);
    random.setLargeFeatureSeed(seed, chunkX, chunkZ);
    random.nextDouble();   // MineshaftStructure.findGenerationPoint head

    detail::MsBuilder b;
    MsPiece room;
    room.kind = MsKind::ROOM;
    room.genDepth = 0;
    room.hasOrientation = false;
    room.box = makeRoomBox(random, (chunkX << 4) + 2, (chunkZ << 4) + 2);
    b.pieces.push_back(room);

    detail::addChildren(b, 0, random);
    // MineshaftStructure.findGenerationPoint: the GenerationStub position is
    // (middleBlockX, 50 + yOffset, minBlockZ) where yOffset is moveBelowSeaLevel's
    // return — the stub the biome gate samples. Expose it for callers.
    const int32_t dy = detail::moveBelowSeaLevel(b, 63, -64, random, 10);
    if (yOffsetOut) *yOffsetOut = dy;
    return b.pieces;
}

} // namespace mc::levelgen::structure::structures
