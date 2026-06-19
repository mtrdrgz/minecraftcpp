#pragma once

// 1:1 port of the PURE vertical-placement integer math in
//   net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder
//   (26.1.2)
//
// StructurePiecesBuilder collects a structure's pieces (each carrying only a
// BoundingBox for the purposes of this math) and offers three vertical-placement
// helpers that every StructureStart leans on to slide an assembled structure up
// or down into the world before it is written:
//
//   offsetPiecesVertically(int dy)                         [25-29]  — translate all pieces by (0,dy,0)
//   moveBelowSeaLevel(seaLevel, minY, RandomSource, offset)[31-43]  — RNG-driven, returns dy
//   moveInsideHeights(RandomSource, lowestAllowed, highest)[45-57]  — RNG-driven, void
//
// All three are pure integer arithmetic over the aggregate bounding box of the
// pieces. There are NO world writes, NO registry/datapack access, NO GL. The two
// RNG-driven helpers draw at most one `RandomSource.nextInt(bound)` each, and
// only under a guard that makes `bound > 0` (so the draw never throws).
//
// The aggregate box `getBoundingBox()` is
//   StructurePiece.createBoundingBox(stream) == BoundingBox.encapsulatingBoxes
// — itself certified byte-exact in StructurePieceCollection.h, which this header
// reuses. A "piece" here is modelled by its BoundingBox alone (exactly what
// `pieces.map(StructurePiece::getBoundingBox)` exposes), and
// StructurePiece.move(0,dy,0) == BoundingBox.move(0,dy,0) (StructurePiece.java
// :510-512), so offsetPiecesVertically mutates each box in place by (0,dy,0).
//
// 1:1 fidelity notes (the int traps):
//   * Java `int` arithmetic wraps on overflow; the BoundingBox.h iadd/isub
//     helpers reproduce that via uint32. getYSpan() == (maxY-minY+1) wrapping,
//     maxY()/minY() are the raw fields. Every +/- below goes through iadd/isub.
//   * RandomSource.nextInt(bound) is the certified Legacy/Xoroshiro stream; the
//     parity gate replays the EXACT draws the real method made (recorded
//     Java-side through a proxy), so this header is verified independent of RNG
//     certification — but a real RandomSource* may also be passed in the engine.
//   * moveBelowSeaLevel: draws iff y1Pos < maxY (=> bound = maxY-y1Pos > 0).
//   * moveInsideHeights: draws iff heightSpan > 1 (=> bound = heightSpan > 0).
//
// The deliberately-unported members of StructurePiecesBuilder are the non-pure
// list bits (addPiece, findCollisionPiece, build, clear, isEmpty) and the box
// aggregate getBoundingBox — addPiece/findCollisionPiece/getBoundingBox already
// live in StructurePieceCollection.h; the rest are trivial container plumbing.
//
// Certified byte-exact by structure_pieces_builder_math_parity
// (tools/StructurePiecesBuilderMathParity.java).

#include "../BoundingBox.h"
#include "../StructurePieceCollection.h"

#include <vector>

namespace mc::levelgen::structure::pieces {

using mc::levelgen::structure::BoundingBox;
using mc::levelgen::structure::iadd;
using mc::levelgen::structure::isub;

// Minimal stand-in for net.minecraft.util.RandomSource as used by these helpers:
// the ONLY draw made is nextInt(int bound). The parity gate plays back the exact
// sequence of values the real RandomSource returned (recorded Java-side), so the
// math is verified without depending on RNG-stream certification. The engine may
// instead supply a wrapper over the real certified LegacyRandomSource.
struct NextIntSource {
    virtual ~NextIntSource() = default;
    // java.util.Random.nextInt(int) contract: bound MUST be > 0 (else IAE).
    virtual int32_t nextInt(int32_t bound) = 0;
};

// A scripted source that returns a fixed list of pre-recorded draws in order.
// Used by the parity gate; mirrors exactly what the real RandomSource produced.
class ScriptedNextIntSource final : public NextIntSource {
public:
    explicit ScriptedNextIntSource(std::vector<int32_t> draws)
        : m_draws(std::move(draws)) {}
    int32_t nextInt(int32_t /*bound*/) override { return m_draws.at(m_index++); }
    std::size_t consumed() const { return m_index; }

private:
    std::vector<int32_t> m_draws;
    std::size_t m_index = 0;
};

// The piece collection: only the per-piece BoundingBox matters to this math.
// Mirrors StructurePiecesBuilder.pieces (the List<StructurePiece>), reduced to
// the boxes that getBoundingBox()/move() touch.
class StructurePiecesBuilderMath {
public:
    StructurePiecesBuilderMath() = default;
    explicit StructurePiecesBuilderMath(std::vector<BoundingBox> boxes)
        : m_boxes(std::move(boxes)) {}

    void addPiece(const BoundingBox& box) { m_boxes.push_back(box); }
    const std::vector<BoundingBox>& boxes() const { return m_boxes; }

    // StructurePiecesBuilder.getBoundingBox() — StructurePiecesBuilder.java:71-73:
    //   return StructurePiece.createBoundingBox(this.pieces.stream());
    // (certified in StructurePieceCollection.h; throws if empty.)
    BoundingBox getBoundingBox() const {
        return mc::levelgen::structure::piece::createBoundingBox(m_boxes);
    }

    // StructurePiecesBuilder.offsetPiecesVertically(int dy) — :25-29.
    //   for (piece : pieces) piece.move(0, dy, 0);
    // StructurePiece.move(0,dy,0) == BoundingBox.move(0,dy,0) (StructurePiece.java
    // :510-512). Mutates each box in place by (0, dy, 0).
    void offsetPiecesVertically(int32_t dy) {
        for (BoundingBox& b : m_boxes) {
            b.move(0, dy, 0);
        }
    }

    // StructurePiecesBuilder.moveBelowSeaLevel(int, int, RandomSource, int)
    // — :31-43. Returns the applied dy; mutates the pieces via
    // offsetPiecesVertically.
    //
    //   int maxY = seaLevel - offset;
    //   BoundingBox boundingBox = this.getBoundingBox();
    //   int y1Pos = boundingBox.getYSpan() + minY + 1;
    //   if (y1Pos < maxY) {
    //      y1Pos += random.nextInt(maxY - y1Pos);
    //   }
    //   int dy = y1Pos - boundingBox.maxY();
    //   this.offsetPiecesVertically(dy);
    //   return dy;
    int32_t moveBelowSeaLevel(int32_t seaLevel, int32_t minY,
                              NextIntSource& random, int32_t offset) {
        int32_t maxY = isub(seaLevel, offset);
        BoundingBox boundingBox = this->getBoundingBox();
        int32_t y1Pos = iadd(iadd(boundingBox.getYSpan(), minY), 1);
        if (y1Pos < maxY) {
            // bound = maxY - y1Pos > 0 here (guard guarantees it).
            y1Pos = iadd(y1Pos, random.nextInt(isub(maxY, y1Pos)));
        }
        int32_t dy = isub(y1Pos, boundingBox.maxY);
        this->offsetPiecesVertically(dy);
        return dy;
    }

    // StructurePiecesBuilder.moveInsideHeights(RandomSource, int, int) — :45-57.
    // void; mutates the pieces via offsetPiecesVertically. Returns the applied dy
    // here purely so the parity gate can observe it (Java returns void, but dy is
    // a local — the post-move box already encodes it, and we expose it too).
    //
    //   BoundingBox boundingBox = this.getBoundingBox();
    //   int heightSpan = highestAllowed - lowestAllowed + 1 - boundingBox.getYSpan();
    //   int y0Pos;
    //   if (heightSpan > 1) {
    //      y0Pos = lowestAllowed + random.nextInt(heightSpan);
    //   } else {
    //      y0Pos = lowestAllowed;
    //   }
    //   int dy = y0Pos - boundingBox.minY();
    //   this.offsetPiecesVertically(dy);
    int32_t moveInsideHeights(NextIntSource& random, int32_t lowestAllowed,
                              int32_t highestAllowed) {
        BoundingBox boundingBox = this->getBoundingBox();
        // highestAllowed - lowestAllowed + 1 - boundingBox.getYSpan(), evaluated
        // strictly left-to-right with wrapping int arithmetic (Java semantics).
        int32_t heightSpan = isub(iadd(isub(highestAllowed, lowestAllowed), 1),
                                  boundingBox.getYSpan());
        int32_t y0Pos;
        if (heightSpan > 1) {
            y0Pos = iadd(lowestAllowed, random.nextInt(heightSpan));
        } else {
            y0Pos = lowestAllowed;
        }
        int32_t dy = isub(y0Pos, boundingBox.minY);
        this->offsetPiecesVertically(dy);
        return dy;
    }

private:
    std::vector<BoundingBox> m_boxes;
};

} // namespace mc::levelgen::structure::pieces
