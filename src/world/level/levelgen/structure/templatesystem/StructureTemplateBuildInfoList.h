#pragma once

// 1:1 C++ port of the PURE block-ordering helper in the REAL decompiled 26.1.2
// class
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
//     private static List<StructureBlockInfo> buildInfoList(
//         List<StructureBlockInfo> fullBlockList,
//         List<StructureBlockInfo> blockEntitiesList,
//         List<StructureBlockInfo> otherBlocksList)            : 156-172
//
// This is the routine StructureTemplate.fillFromWorld() runs at capture time to
// produce the canonical ordering of a palette's block list. Every template the
// jigsaw / scattered-feature systems place is iterated in THIS order, so the
// ordering is load-bearing for placement-RNG parity downstream (data markers,
// jigsaw selection, etc. all walk the list in this order).
//
// The method is fully self-contained PURE ordering over the BLOCK POSITIONS:
//   1. sort each of the three input lists by the comparator
//          comparingInt(pos.getY()).thenComparingInt(pos.getX()).thenComparingInt(pos.getZ())
//      i.e. (Y, then X, then Z), ascending, as SIGNED 32-bit ints.
//   2. concatenate, in this exact order:  fullBlockList ++ otherBlocksList ++ blockEntitiesList
//      (NOTE: the parameter order is full, blockEntities, other — but the
//       concatenation order is full, OTHER, blockEntities; the middle two are
//       swapped relative to the parameter list).
// There are NO world reads, NO RandomSource, NO registry/datapack lookups. Only
// the BlockPos of each StructureBlockInfo participates; the BlockState and NBT
// are carried through untouched.
//
// 1:1 TRAPS this gate locks down:
//   - The comparator key order is (Y, X, Z), NOT (X, Y, Z) or (Z, Y, X). Getting
//     the primary key wrong reshuffles every multi-layer template.
//   - java.util.List.sort is a STABLE sort (TimSort): elements that compare equal
//     (identical Y,X,Z) keep their original relative order. We must use
//     std::stable_sort, NOT std::sort, or duplicate-position ties diverge.
//   - The concatenation order is full, other, blockEntities — the block-entities
//     list comes LAST, and the "other" list is in the MIDDLE even though it is the
//     THIRD parameter. A naive "concat in parameter order" is wrong.
//   - The comparison is signed 32-bit (comparingInt on int coordinates), so a
//     negative Y sorts below a positive Y. We compare the int32_t coordinates
//     directly (C++ signed comparison matches Java's).
//
// Certified byte-exact by structure_template_build_info_list_parity
// (tools/StructureTemplateBuildInfoListParity.java reflectively drives the REAL
//  private static StructureTemplate.buildInfoList over the REAL StructureBlockInfo
//  records, tagging each input with a unique id carried in its NBT, and emits the
//  resulting id sequence; this header recomputes the same ordering and compares).

#include <algorithm>
#include <cstdint>
#include <vector>

namespace mc::levelgen::structure::templatesystem {

// Minimal stand-in for net.minecraft...StructureTemplate.StructureBlockInfo.
// Only the fields buildInfoList actually reads (the BlockPos) plus a unique `id`
// used to observe the resulting permutation are modelled; the real record also
// carries BlockState + CompoundTag, which buildInfoList passes through verbatim
// and which therefore do not affect ordering.
struct BlockInfo {
    int32_t x;
    int32_t y;
    int32_t z;
    int32_t id; // identity tag, carried through unchanged (mirrors nbt "id")
};

// Comparator<StructureBlockInfo>
//   comparingInt(o -> o.pos.getY())
//     .thenComparingInt(o -> o.pos.getX())
//     .thenComparingInt(o -> o.pos.getZ())
// Returns true iff `a` must sort strictly before `b`. With std::stable_sort this
// reproduces Java List.sort's stable ordering for equal keys.
inline bool blockInfoLess(const BlockInfo& a, const BlockInfo& b) noexcept {
    if (a.y != b.y) return a.y < b.y;
    if (a.x != b.x) return a.x < b.x;
    return a.z < b.z;
}

// buildInfoList(fullBlockList, blockEntitiesList, otherBlocksList) : 156-172.
//
// Mirrors the real method exactly: each of the three lists is sorted IN PLACE
// (here on copies) with the (Y,X,Z) comparator via a stable sort, then the result
// is `fullBlockList` ++ `otherBlocksList` ++ `blockEntitiesList`.
inline std::vector<BlockInfo> buildInfoList(std::vector<BlockInfo> fullBlockList,
                                            std::vector<BlockInfo> blockEntitiesList,
                                            std::vector<BlockInfo> otherBlocksList) {
    std::stable_sort(fullBlockList.begin(), fullBlockList.end(), blockInfoLess);
    std::stable_sort(otherBlocksList.begin(), otherBlocksList.end(), blockInfoLess);
    std::stable_sort(blockEntitiesList.begin(), blockEntitiesList.end(), blockInfoLess);

    std::vector<BlockInfo> blockInfoList;
    blockInfoList.reserve(fullBlockList.size() + otherBlocksList.size() + blockEntitiesList.size());
    blockInfoList.insert(blockInfoList.end(), fullBlockList.begin(), fullBlockList.end());
    blockInfoList.insert(blockInfoList.end(), otherBlocksList.begin(), otherBlocksList.end());
    blockInfoList.insert(blockInfoList.end(), blockEntitiesList.begin(), blockEntitiesList.end());
    return blockInfoList;
}

} // namespace mc::levelgen::structure::templatesystem
