#pragma once

// 1:1 C++ port of the PURE aggregation / corner-iteration / identity helpers of
//   net.minecraft.world.level.levelgen.structure.BoundingBox (26.1.2)
// that the sibling bounding_box gate (BoundingBox.h / BoundingBoxParity.java)
// deliberately leaves UNPORTED (see BoundingBox.h:35-39). Those skipped members
// are exactly:
//
//   * static Optional<BoundingBox> encapsulatingPositions(Iterable<BlockPos>) [127-136]
//   * (instance, mutating) BoundingBox encapsulate(BoundingBox)              [150-159]
//   * (instance, mutating) BoundingBox move(int,int,int)                     [184-192]
//   * void forAllCorners(Consumer<BlockPos>)                                 [241-251]
//   * int  hashCode()  ( == Objects.hash(minX,minY,minZ,maxX,maxY,maxZ) )    [281-284]
//   * boolean equals(Object)  (six-field compare)                           [265-279]
//
// Every one is self-contained: NO world writes, NO RandomSource, NO
// registry/datapack, NO GL. `encapsulate(BoundingBox)` and `move(int,int,int)`
// are already implemented in BoundingBox.h but were never exercised by the
// bounding_box gate; the two genuinely-unported quantities are forAllCorners,
// encapsulatingPositions, hashCode and equals, ported here.
//
// 1:1 TRAPS faithfully reproduced:
//   * forAllCorners (BoundingBox.java:241-251) visits the EIGHT corners in a
//     FIXED order: (maxX,maxY,maxZ),(minX,maxY,maxZ),(maxX,minY,maxZ),
//     (minX,minY,maxZ),(maxX,maxY,minZ),(minX,maxY,minZ),(maxX,minY,minZ),
//     (minX,minY,minZ). It reuses one MutableBlockPos, but the consumer here
//     copies each value, so the order — not aliasing — is what the gate checks.
//   * hashCode == java.util.Objects.hash(Integer...) which delegates to
//     java.util.Arrays.hashCode(Object[]): result = 1; for each boxed int e:
//       result = 31 * result + e.hashCode();      // Integer.hashCode(v) == v
//     so hash = ((((((1*31+minX)*31+minY)*31+minZ)*31+maxX)*31+maxY)*31+maxZ),
//     all in 32-bit int with two's-complement overflow. The multiply/add must
//     wrap exactly like Java int (C++ signed overflow is UB at -O2) — routed
//     through uint32_t.
//   * encapsulate(BoundingBox) folds Math.min/Math.max over each axis IN PLACE
//     and returns the SAME object (the seed of encapsulatingPositions). Because
//     the seed box is constructed from a single BlockPos (min==max==pos), the
//     subsequent folds can only grow it; we keep Math.min/max via imin/imax.
//   * encapsulatingPositions seeds from the FIRST position (new BoundingBox(pos)
//     == a zero-volume box at pos) and folds encapsulate over the rest; an EMPTY
//     iterable yields Optional.empty() (modelled here as the bool `present`).
//
// Certified byte-exact by bounding_box_aggregate_parity
// (tools/BoundingBoxAggregateParity.java).

#include "BoundingBox.h"

#include <array>
#include <cstdint>

namespace mc::levelgen::structure {

// BoundingBox.java:281-284 — hashCode() == Objects.hash(minX,minY,minZ,maxX,maxY,
// maxZ). Arrays.hashCode seed is 1; each boxed Integer hashes to its own value.
// All arithmetic is 32-bit Java int (wrapping) — done in uint32_t then cast back.
inline int32_t boundingBoxHashCode(const BoundingBox& b) noexcept {
    uint32_t result = 1u;
    const int32_t fields[6] = {b.minX, b.minY, b.minZ, b.maxX, b.maxY, b.maxZ};
    for (int i = 0; i < 6; ++i) {
        result = 31u * result + static_cast<uint32_t>(fields[i]);
    }
    return static_cast<int32_t>(result);
}

// BoundingBox.java:265-279 — equals(Object): true iff the other is a BoundingBox
// with all six fields equal. (operator== on the struct is the same compare.)
inline bool boundingBoxEquals(const BoundingBox& a, const BoundingBox& b) noexcept {
    return a.minX == b.minX && a.minY == b.minY && a.minZ == b.minZ
        && a.maxX == b.maxX && a.maxY == b.maxY && a.maxZ == b.maxZ;
}

// BoundingBox.java:241-251 — forAllCorners(Consumer<BlockPos>): the eight corners
// in their fixed visitation order. Returned as an ordered array so the gate can
// check the SEQUENCE byte-for-byte.
inline std::array<Vec3i, 8> boundingBoxCorners(const BoundingBox& b) noexcept {
    return {{
        {b.maxX, b.maxY, b.maxZ},
        {b.minX, b.maxY, b.maxZ},
        {b.maxX, b.minY, b.maxZ},
        {b.minX, b.minY, b.maxZ},
        {b.maxX, b.maxY, b.minZ},
        {b.minX, b.maxY, b.minZ},
        {b.maxX, b.minY, b.minZ},
        {b.minX, b.minY, b.minZ},
    }};
}

// BoundingBox.java:127-136 — encapsulatingPositions(Iterable<BlockPos>): seed a
// zero-volume box at the first position, then encapsulate the rest in order.
// `present=false` models Optional.empty() for an empty input. The C++ caller
// passes the positions as a span/vector; iteration order matches Java's.
struct EncapsulatingResult {
    bool present{false};
    BoundingBox box{};
};

inline EncapsulatingResult encapsulatingPositions(const Vec3i* positions, std::size_t count) noexcept {
    if (count == 0) return {};
    // new BoundingBox(BlockPos) == BoundingBox(x,y,z,x,y,z) — a point box.
    BoundingBox result(positions[0].x, positions[0].y, positions[0].z,
                       positions[0].x, positions[0].y, positions[0].z);
    for (std::size_t i = 1; i < count; ++i) {
        result.encapsulate(positions[i]); // mutating fold (BoundingBox.h:217)
    }
    return {true, result};
}

} // namespace mc::levelgen::structure
