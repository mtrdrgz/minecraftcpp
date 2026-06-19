#pragma once

// 1:1 C++ port of the PURE placement-coordinate helpers in the REAL decompiled
// 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//     BlockPos calculateConnectedPosition(StructurePlaceSettings s1, BlockPos c1,
//                                         StructurePlaceSettings s2, BlockPos c2)   :239-245
//     static BlockPos calculateRelativePosition(StructurePlaceSettings s, BlockPos p) :247-249
//
// These are the coordinate helpers the jigsaw system (JigsawPlacement) uses to
// connect two pool elements at their matching jigsaw-block markers: it transforms
// each connection point into its element's placed orientation, then takes the
// vector difference. They are fully self-contained PURE integer geometry that
// reads ONLY the mirror / rotation / rotationPivot off each StructurePlaceSettings.
// There are NO world writes, NO RandomSource, NO registry/datapack, NO BlockState.
//
// They are built on StructureTemplate.transform(BlockPos, Mirror, Rotation, pivot)
// (certified by structure_transform_parity / StructureTransforms.h) and a final
//   BlockPos.subtract(Vec3i) == offset(-vx,-vy,-vz)   (Vec3i.java:100-102,
//                                                       BlockPos.java:138-140 / :120-122)
//
// Certified byte-exact by structure_connected_position_parity
// (tools/StructureConnectedPositionParity.java drives the REAL StructureTemplate
//  instance method + REAL StructurePlaceSettings via reflection, emits a TSV;
//  this header recomputes and compares).
//
// 1:1 TRAPS this gate locks down:
//   - calculateRelativePosition is `transform(pos, settings.getMirror(),
//     settings.getRotation(), settings.getRotationPivot())` — the pivot is the
//     SETTINGS' pivot, NOT a fresh BlockPos.ZERO; using the wrong pivot silently
//     mis-places every connected element.
//   - the final subtract is `markerPos1 - markerPos2` (order matters; it is NOT
//     symmetric and NOT markerPos2 - markerPos1).
//   - BlockPos.subtract(v) is offset(-v.x,-v.y,-v.z); `-v.x` itself is a Java int
//     negation that WRAPS at Integer.MIN_VALUE (−(−2^31) == −2^31), and the add in
//     offset wraps in two's complement. C++ signed overflow is UB, so we route the
//     negate+add through uint32_t to stay byte-identical for every input.
//   - BlockPos.offset short-circuits to `this` when (x,y,z)==(0,0,0); that returns
//     the SAME coordinates (no clone semantics matter for value comparison), so the
//     plain element-wise subtract below is faithful.

#include <cstdint>

#include "StructureTransforms.h"  // mc::levelgen::structure::{BlockPos, Mirror, Rotation, structureTransform}

namespace mc::levelgen::structure::templatesystem {

using mc::levelgen::structure::BlockPos;
using mc::levelgen::structure::Mirror;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::structureTransform;

// net.minecraft.world.level.levelgen.structure.templatesystem.StructurePlaceSettings
// — only the three fields these helpers read. Defaults match the REAL class
// (StructurePlaceSettings.java:15-17): NONE / NONE / BlockPos.ZERO.
struct StructurePlaceSettings {
    Mirror mirror = Mirror::NONE;
    Rotation rotation = Rotation::NONE;
    BlockPos rotationPivot{0, 0, 0};

    constexpr Mirror getMirror() const noexcept { return mirror; }
    constexpr Rotation getRotation() const noexcept { return rotation; }
    constexpr const BlockPos& getRotationPivot() const noexcept { return rotationPivot; }
};

// Java int arithmetic wraps on overflow (two's complement). C++ signed overflow is
// UB, so route the negate+add through uint32_t and reinterpret. This keeps the port
// byte-identical to Java even at Integer.MIN/MAX_VALUE.
constexpr int32_t iadd(int32_t a, int32_t b) noexcept {
    return static_cast<int32_t>(static_cast<uint32_t>(a) + static_cast<uint32_t>(b));
}
constexpr int32_t ineg(int32_t a) noexcept {
    return static_cast<int32_t>(0u - static_cast<uint32_t>(a));
}

// BlockPos.subtract(Vec3i) — BlockPos.java:138-140 == offset(-v.x,-v.y,-v.z).
constexpr BlockPos blockPosSubtract(const BlockPos& a, const BlockPos& b) noexcept {
    return BlockPos{iadd(a.x, ineg(b.x)), iadd(a.y, ineg(b.y)), iadd(a.z, ineg(b.z))};
}

// StructureTemplate.calculateRelativePosition(settings, pos) — StructureTemplate.java:247-249.
constexpr BlockPos calculateRelativePosition(const StructurePlaceSettings& settings, const BlockPos& pos) noexcept {
    return structureTransform(pos, settings.getMirror(), settings.getRotation(), settings.getRotationPivot());
}

// StructureTemplate.calculateConnectedPosition(s1, c1, s2, c2) — StructureTemplate.java:239-245.
constexpr BlockPos calculateConnectedPosition(const StructurePlaceSettings& settings1, const BlockPos& connection1,
                                              const StructurePlaceSettings& settings2, const BlockPos& connection2) noexcept {
    BlockPos markerPos1 = calculateRelativePosition(settings1, connection1);
    BlockPos markerPos2 = calculateRelativePosition(settings2, connection2);
    return blockPosSubtract(markerPos1, markerPos2);
}

} // namespace mc::levelgen::structure::templatesystem
