// 1:1 port of the PURE integer math in net.minecraft.client.renderer.Octree
// (Minecraft Java Edition 26.1.2) — the section-render-octree spatial index used
// by LevelRenderer for frustum-ordered chunk traversal.
//
// Octree itself loads GL-free (it only imports SectionRenderDispatcher, Frustum,
// core pos types, Mth, BoundingBox, AABB — no com.mojang.blaze3d / GL / GLFW), so
// this header mirrors only the deterministic int/bool math that fully determines
// the tree's geometry and node ordering. Nothing here touches a GPU, a window, or
// any RenderSystem state.
//
// Ported pieces (each is a verbatim translation of the Java, same operators, same
// order, same int truncation toward zero):
//   * rootBoundingBox(..)      — the Octree(SectionPos,renderDistance,sectionsPerChunk,
//                                 minBlockY) constructor body that builds the root BB.
//   * AxisSorting + getAxisSorting(..) — the camera-axis dominance ordering enum.
//   * Branch(boundingBox)      — bbCenterX/Y/Z, sorting, camera*DiffNegative.
//   * getNodeIndex(..)         — child-octant index from the per-axis "opposite side"
//                                flags and the branch's AxisSorting shifts.
//   * createChildBoundingBox(..) — the half-space subdivision of a branch's BB.
//   * areChildrenLeaves()      — getXSpan()==32 leaf test.
//   * isClose(..)              — the close-distance double comparison.
//
// Helper deps mirrored inline (all pure):
//   * Mth.smallestEncompassingPowerOfTwo(int)  (Mth.java)
//   * SectionPos.sectionToBlockCoord(int) = sectionCoord << 4   (SectionPos.java)
//   * SectionPos.origin()/center() block coords                 (SectionPos.java)
//   * BoundingBox.getXSpan()/getYSpan()/getZSpan() = max-min+1   (BoundingBox.java)
//
// Certified bit-for-bit by render/OctreeParityTest.cpp against tools/OctreeParity.java
// (which drives the REAL Octree via reflection).

#pragma once

#include <cstdint>
#include <cstdlib>

namespace mc::render::octree {

// ---- net.minecraft.world.level.levelgen.structure.BoundingBox (int fields) ----
// Only the integer span/extent surface Octree uses.
struct BoundingBox {
    int minX, minY, minZ, maxX, maxY, maxZ;

    // BoundingBox.getXSpan() = this.maxX - this.minX + 1; (likewise Y/Z)
    int getXSpan() const { return maxX - minX + 1; }
    int getYSpan() const { return maxY - minY + 1; }
    int getZSpan() const { return maxZ - minZ + 1; }
};

// ---- net.minecraft.util.Mth.smallestEncompassingPowerOfTwo(int) ----
// VERBATIM: int result = input - 1; result |= result >> 1; ... ; return result + 1;
// (Java >> on int is arithmetic; for the inputs used here input >= 1 so result >= 0
//  and the shifts behave as the unsigned bit-fill they're written to be.)
inline int smallestEncompassingPowerOfTwo(int input) {
    int result = input - 1;
    result |= result >> 1;
    result |= result >> 2;
    result |= result >> 4;
    result |= result >> 8;
    result |= result >> 16;
    return result + 1;
}

// ---- net.minecraft.core.SectionPos.sectionToBlockCoord(int) = sectionCoord << 4 ----
inline int sectionToBlockCoord(int sectionCoord) { return sectionCoord << 4; }

// Octree(final SectionPos cameraSection, final int renderDistance,
//        final int sectionsPerChunk, final int minBlockY)
//
// We take the camera SECTION coords directly (sectionX/Y/Z). In Java:
//   cameraSectionOrigin = cameraSection.origin()    -> sectionToBlockCoord(sec.{x,y,z}())
//   this.cameraSectionCenter = cameraSection.center() -> origin().offset(8,8,8)
// Both are reproduced from the section coords below. Returns the root BoundingBox.
inline BoundingBox rootBoundingBox(int cameraSectionX, int cameraSectionY, int cameraSectionZ,
                                   int renderDistance, int sectionsPerChunk, int minBlockY) {
    int visibleAreaDiameterInSections = renderDistance * 2 + 1;
    int boundingBoxSizeInSections = smallestEncompassingPowerOfTwo(visibleAreaDiameterInSections);
    int distanceToBBEdgeInBlocks = renderDistance * 16;

    // cameraSectionOrigin = cameraSection.origin() (block coords)
    int originX = sectionToBlockCoord(cameraSectionX);
    int originY = sectionToBlockCoord(cameraSectionY);
    int originZ = sectionToBlockCoord(cameraSectionZ);

    int minX = originX - distanceToBBEdgeInBlocks;
    int maxX = minX + boundingBoxSizeInSections * 16 - 1;
    int minY = boundingBoxSizeInSections >= sectionsPerChunk ? minBlockY : originY - distanceToBBEdgeInBlocks;
    int maxY = minY + boundingBoxSizeInSections * 16 - 1;
    int minZ = originZ - distanceToBBEdgeInBlocks;
    int maxZ = minZ + boundingBoxSizeInSections * 16 - 1;
    return BoundingBox{minX, minY, minZ, maxX, maxY, maxZ};
}

// cameraSection.center() block coords = origin().offset(8,8,8).
inline int cameraCenterX(int cameraSectionX) { return sectionToBlockCoord(cameraSectionX) + 8; }
inline int cameraCenterY(int cameraSectionY) { return sectionToBlockCoord(cameraSectionY) + 8; }
inline int cameraCenterZ(int cameraSectionZ) { return sectionToBlockCoord(cameraSectionZ) + 8; }

// ---- private enum Octree.AxisSorting ----
// Declaration order matters: it fixes each constant's xShift/yShift/zShift AND its
// ordinal (which is what getAxisSorting returns / how it is identified downstream).
enum class AxisSorting : int {
    XYZ = 0,  // (4, 2, 1)
    XZY,      // (4, 1, 2)
    YXZ,      // (2, 4, 1)
    YZX,      // (1, 4, 2)
    ZXY,      // (2, 1, 4)
    ZYX,      // (1, 2, 4)
};

inline int axisShiftX(AxisSorting s) {
    switch (s) {
        case AxisSorting::XYZ: return 4;
        case AxisSorting::XZY: return 4;
        case AxisSorting::YXZ: return 2;
        case AxisSorting::YZX: return 1;
        case AxisSorting::ZXY: return 2;
        case AxisSorting::ZYX: return 1;
    }
    return 0;
}
inline int axisShiftY(AxisSorting s) {
    switch (s) {
        case AxisSorting::XYZ: return 2;
        case AxisSorting::XZY: return 1;
        case AxisSorting::YXZ: return 4;
        case AxisSorting::YZX: return 4;
        case AxisSorting::ZXY: return 1;
        case AxisSorting::ZYX: return 2;
    }
    return 0;
}
inline int axisShiftZ(AxisSorting s) {
    switch (s) {
        case AxisSorting::XYZ: return 1;
        case AxisSorting::XZY: return 2;
        case AxisSorting::YXZ: return 1;
        case AxisSorting::YZX: return 2;
        case AxisSorting::ZXY: return 4;
        case AxisSorting::ZYX: return 4;
    }
    return 0;
}

// public static Octree.AxisSorting getAxisSorting(absXDiff, absYDiff, absZDiff)
inline AxisSorting getAxisSorting(int absXDiff, int absYDiff, int absZDiff) {
    if (absXDiff > absYDiff && absXDiff > absZDiff) {
        return absYDiff > absZDiff ? AxisSorting::XYZ : AxisSorting::XZY;
    } else if (absYDiff > absXDiff && absYDiff > absZDiff) {
        return absXDiff > absZDiff ? AxisSorting::YXZ : AxisSorting::YZX;
    } else {
        return absXDiff > absYDiff ? AxisSorting::ZXY : AxisSorting::ZYX;
    }
}

// private static int Branch.getNodeIndex(sorting, xOpp, yOpp, zOpp)
inline int getNodeIndex(AxisSorting sorting, bool xDiffsOppositeSides, bool yDiffsOppositeSides,
                        bool zDiffsOppositeSides) {
    int index = 0;
    if (xDiffsOppositeSides) index += axisShiftX(sorting);
    if (yDiffsOppositeSides) index += axisShiftY(sorting);
    if (zDiffsOppositeSides) index += axisShiftZ(sorting);
    return index;
}

// ---- private class Octree.Branch (its derived geometry) ----
struct Branch {
    BoundingBox boundingBox;
    int bbCenterX, bbCenterY, bbCenterZ;
    AxisSorting sorting;
    bool cameraXDiffNegative, cameraYDiffNegative, cameraZDiffNegative;

    // Branch(BoundingBox): center = min + span/2; sorting/diff flags from camera center.
    static Branch make(const BoundingBox& bb, int cameraCenterX, int cameraCenterY, int cameraCenterZ) {
        Branch b;
        b.boundingBox = bb;
        b.bbCenterX = bb.minX + bb.getXSpan() / 2;
        b.bbCenterY = bb.minY + bb.getYSpan() / 2;
        b.bbCenterZ = bb.minZ + bb.getZSpan() / 2;
        int cameraXDiff = cameraCenterX - b.bbCenterX;
        int cameraYDiff = cameraCenterY - b.bbCenterY;
        int cameraZDiff = cameraCenterZ - b.bbCenterZ;
        b.sorting = getAxisSorting(std::abs(cameraXDiff), std::abs(cameraYDiff), std::abs(cameraZDiff));
        b.cameraXDiffNegative = cameraXDiff < 0;
        b.cameraYDiffNegative = cameraYDiff < 0;
        b.cameraZDiffNegative = cameraZDiff < 0;
        return b;
    }

    // private boolean areChildrenLeaves() { return getXSpan() == 32; }
    bool areChildrenLeaves() const { return boundingBox.getXSpan() == 32; }

    // private BoundingBox createChildBoundingBox(sectionXDiffNegative, ...)
    BoundingBox createChildBoundingBox(bool sectionXDiffNegative, bool sectionYDiffNegative,
                                       bool sectionZDiffNegative) const {
        int minX, maxX;
        if (sectionXDiffNegative) { minX = boundingBox.minX; maxX = bbCenterX - 1; }
        else                      { minX = bbCenterX;        maxX = boundingBox.maxX; }
        int minY, maxY;
        if (sectionYDiffNegative) { minY = boundingBox.minY; maxY = bbCenterY - 1; }
        else                      { minY = bbCenterY;        maxY = boundingBox.maxY; }
        int minZ, maxZ;
        if (sectionZDiffNegative) { minZ = boundingBox.minZ; maxZ = bbCenterZ - 1; }
        else                      { minZ = bbCenterZ;        maxZ = boundingBox.maxZ; }
        return BoundingBox{minX, minY, minZ, maxX, maxY, maxZ};
    }
};

// private boolean Octree.isClose(minX,minY,minZ,maxX,maxY,maxZ, closeDistance)
// cameraSectionCenter is the integer center; the comparisons are int-vs-double in
// Java but every operand here is an exact integer value, so a plain integer
// comparison is bit-identical.
inline bool isClose(int cameraCenterX, int cameraCenterY, int cameraCenterZ, double minX, double minY,
                    double minZ, double maxX, double maxY, double maxZ, int closeDistance) {
    return cameraCenterX > minX - closeDistance && cameraCenterX < maxX + closeDistance &&
           cameraCenterY > minY - closeDistance && cameraCenterY < maxY + closeDistance &&
           cameraCenterZ > minZ - closeDistance && cameraCenterZ < maxZ + closeDistance;
}

}  // namespace mc::render::octree
