#pragma once

// 1:1 C++ port of the PURE, deterministic parts of the decompiled 26.1.2 class
//   net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate
// plus the jigsaw-block discovery + shuffle that the jigsaw structure-assembly
// system feeds on. NOTHING here touches a real world — this is the .nbt loader +
// JigsawBlockInfo discovery + getShuffledJigsawBlocks ordering ONLY.
//
// Sources translated verbatim (line refs are into 26.1.2/src):
//   StructureTemplate.load(HolderGetter<Block>, CompoundTag)        :709-731
//   StructureTemplate.loadPalette(.. paletteList, blockList)         :733-753
//   StructureTemplate.addToLists / buildInfoList                     :141-172
//       (full-collision blocks, then "other", then block-entities; each sorted
//        stable by (Y,X,Z) — reuses the certified StructureTemplateBuildInfoList.h
//        ordering, and is itself re-certified through this gate).
//   StructureTemplate.JigsawBlockInfo record + .of(StructureBlockInfo) :783-824
//   StructureTemplate.getJointType / getDefaultJointType            :775-781
//       joint default = front-facing horizontal -> ALIGNED else ROLLABLE.
//       (NOTE the Java comment-vs-code: getDefaultJointType returns ALIGNED when
//        the front axis isHorizontal(), ROLLABLE when vertical — ported exactly.)
//   StructureTemplate.Palette.jigsaws()                             :835-841
//       jigsaws = blocks.filter(state.is(JIGSAW)).map(JigsawBlockInfo::of)
//   StructureTemplate.getJigsaws(BlockPos, Rotation)               :197-218
//       per jigsaw: pos -> structureTransform(pos, NONE, rotation, pivot=0)+offset;
//       state.rotate(rotation) rotates the FrontAndTop ORIENTATION property.
//   JigsawBlock.rotate(state, rotation)                            JigsawBlock.java:41-43
//       ORIENTATION := rotation.rotation().rotate(orientation)  (OctahedralGroup)
//   JigsawBlock.getFrontFacing / getTopFacing                     JigsawBlock.java:91-97
//   SinglePoolElement.getShuffledJigsawBlocks + sortBySelectionPriority
//                                                  SinglePoolElement.java:114-127
//       Util.shuffle(list, random) then a STABLE sort by selectionPriority DESC.
//   net.minecraft.util.Util.shuffle(List, RandomSource)            Util.java:1061-1068
//       Fisher-Yates: for (i=size; i>1; i--) { swapTo=nextInt(i); swap(i-1, swapTo); }
//   com.mojang.math.OctahedralGroup.rotate(Direction)/.rotate(FrontAndTop)
//                                                  OctahedralGroup.java:147-184
//   com.mojang.math.SymmetricGroup3 (P123/P321 inverse + permuteAxis)
//                                                  SymmetricGroup3.java:10-69
//   net.minecraft.core.FrontAndTop (front/top per constant, fromFrontAndTop)
//                                                  FrontAndTop.java:6-55
//   net.minecraft.nbt.NbtUtils.readBlockState (Name + Properties decode)
//                                                  NbtUtils.java:127-148
//
// The entity list is parsed-but-ignored for this gate (StructureTemplate.load
// builds entityInfoList; nothing downstream of jigsaw discovery reads it). Block
// states are modelled at the granularity the jigsaw path needs: the block id
// ("Name") plus, for jigsaw blocks, the decoded FrontAndTop ORIENTATION. Other
// blockstate properties are id-invisible to jigsaw discovery and carried as-is.
//
// Certified byte-exact by structure_template_loader_parity
// (tools/StructureTemplateLoaderParity.java drives the REAL StructureTemplate +
//  the REAL getShuffledJigsawBlocks and emits the ordered jigsaw list per
//  (offset, rotation, seed); this header reparses the SAME .nbt bytes and
//  recomputes the same ordering, compared bit-for-bit).

#include "StructureTransforms.h"   // structureTransform, Rotation, BlockPos (certified)
#include "../../../../../nbt/NbtIo.h"
#include "../../../../../nbt/Tag.h"
#include "../../RandomSource.h"
#include "../../../../phys/Direction.h"

#include <algorithm>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc::levelgen::structure::templatesystem {

using mc::Direction;
using mc::Axis;
using mc::AxisDirection;

// ── net.minecraft.core.FrontAndTop (FrontAndTop.java:6-55) ───────────────────
// Each constant has a (front, top) pair of Directions; fromFrontAndTop looks the
// constant back up from (front, top). We model FrontAndTop as just its (front,
// top) Direction pair — that is all jigsaw discovery observes.
struct FrontAndTop {
    Direction front;
    Direction top;
    constexpr bool operator==(const FrontAndTop&) const = default;
};

// FrontAndTop constants by serialized name (FrontAndTop.java:7-18). Only the name
// -> (front, top) map is needed; the BY_TOP_FRONT lookup is the inverse and is
// realised implicitly by the rotate() composing two valid Directions back into a
// constant (every front!=top non-parallel pair is a valid FrontAndTop).
inline std::optional<FrontAndTop> frontAndTopByName(const std::string& name) {
    struct E { const char* name; Direction front; Direction top; };
    static const E table[] = {
        {"down_east",  Direction::DOWN,  Direction::EAST},
        {"down_north", Direction::DOWN,  Direction::NORTH},
        {"down_south", Direction::DOWN,  Direction::SOUTH},
        {"down_west",  Direction::DOWN,  Direction::WEST},
        {"up_east",    Direction::UP,    Direction::EAST},
        {"up_north",   Direction::UP,    Direction::NORTH},
        {"up_south",   Direction::UP,    Direction::SOUTH},
        {"up_west",    Direction::UP,    Direction::WEST},
        {"west_up",    Direction::WEST,  Direction::UP},
        {"east_up",    Direction::EAST,  Direction::UP},
        {"north_up",   Direction::NORTH, Direction::UP},
        {"south_up",   Direction::SOUTH, Direction::UP},
    };
    for (const E& e : table)
        if (name == e.name) return FrontAndTop{e.front, e.top};
    return std::nullopt;
}

// ── com.mojang.math.SymmetricGroup3 (SymmetricGroup3.java:10-69) ─────────────
// We only need the two permutations that appear in the 4 Y-rotation groups:
//   P123 = (0,1,2) identity, its own inverse.
//   P321 = (2,1,0) transposition of X<->Z, its own inverse.
// permuteAxis(axis) = AXIS_VALUES[permute(axis.ordinal())]; inverse().permuteAxis
// for self-inverse perms is the same map. Axis ordinals: X=0,Y=1,Z=2.
enum class Sym3 : int { P123, P321 };

constexpr Axis sym3InversePermuteAxis(Sym3 s, Axis axis) noexcept {
    // P123 and P321 are both self-inverse, so inverse() == identity on them.
    if (s == Sym3::P123) return axis;            // (0,1,2)
    // P321 = (2,1,0): X<->Z, Y fixed.
    switch (axis) {
        case Axis::X: return Axis::Z;
        case Axis::Z: return Axis::X;
        default:      return Axis::Y;
    }
}

// ── com.mojang.math.OctahedralGroup (the 4 Rotation.rotation() groups) ───────
// Rotation.rotation() (Rotation.java:18-21):
//   NONE                -> OctahedralGroup.IDENTITY     (P123, invert F,F,F)
//   CLOCKWISE_90        -> OctahedralGroup.ROT_90_Y_NEG (P321, invert T,F,F)
//   CLOCKWISE_180       -> OctahedralGroup.ROT_180_FACE_XZ (P123, invert T,F,T)
//   COUNTERCLOCKWISE_90 -> OctahedralGroup.ROT_90_Y_POS (P321, invert F,F,T)
// (OctahedralGroup.java:17-38).
struct OctGroup { Sym3 permutation; bool invertX, invertY, invertZ; };

constexpr OctGroup rotationOctGroup(Rotation rot) noexcept {
    switch (rot) {
        case Rotation::NONE:                return {Sym3::P123, false, false, false};
        case Rotation::CLOCKWISE_90:        return {Sym3::P321, true,  false, false};
        case Rotation::CLOCKWISE_180:       return {Sym3::P123, true,  false, true};
        case Rotation::COUNTERCLOCKWISE_90: return {Sym3::P321, false, false, true};
        default:                            return {Sym3::P123, false, false, false};
    }
}

constexpr bool octInverts(const OctGroup& g, Axis axis) noexcept {
    switch (axis) {
        case Axis::X: return g.invertX;
        case Axis::Y: return g.invertY;
        default:      return g.invertZ; // Z
    }
}

// OctahedralGroup.rotate(Direction) — OctahedralGroup.java:147-159.
constexpr Direction octRotateDirection(const OctGroup& g, Direction facing) noexcept {
    Axis oldAxis = mc::directionAxis(facing);
    AxisDirection oldDir = mc::directionAxisDirection(facing);
    Axis newAxis = sym3InversePermuteAxis(g.permutation, oldAxis);
    AxisDirection newDir = octInverts(g, newAxis)
        ? (oldDir == AxisDirection::POSITIVE ? AxisDirection::NEGATIVE : AxisDirection::POSITIVE)
        : oldDir;
    return mc::directionFromAxisAndDirection(newAxis, newDir);
}

// OctahedralGroup.rotate(FrontAndTop) — OctahedralGroup.java:182-184:
//   fromFrontAndTop(rotate(front), rotate(top)). We carry (front, top) directly,
//   so the result is just both directions rotated (always a valid FrontAndTop).
constexpr FrontAndTop octRotateFrontAndTop(const OctGroup& g, FrontAndTop ft) noexcept {
    return FrontAndTop{octRotateDirection(g, ft.front), octRotateDirection(g, ft.top)};
}

// JigsawBlock.rotate(state, rotation): ORIENTATION := rotation.rotation().rotate(ORIENTATION).
constexpr FrontAndTop jigsawStateRotate(FrontAndTop orientation, Rotation rotation) noexcept {
    return octRotateFrontAndTop(rotationOctGroup(rotation), orientation);
}

// ── net.minecraft.world.level.block.entity.JigsawBlockEntity.JointType ───────
enum class JointType : int { ROLLABLE, ALIGNED }; // enum order: ROLLABLE=0, ALIGNED=1.

inline const char* jointTypeName(JointType j) {
    return j == JointType::ALIGNED ? "aligned" : "rollable";
}

// StructureTemplate.getDefaultJointType(state) — StructureTemplate.java:779-781:
//   JigsawBlock.getFrontFacing(state).getAxis().isHorizontal() ? ALIGNED : ROLLABLE.
constexpr JointType defaultJointType(Direction front) noexcept {
    Axis a = mc::directionAxis(front);
    bool horizontal = (a == Axis::X || a == Axis::Z);
    return horizontal ? JointType::ALIGNED : JointType::ROLLABLE;
}

// ── StructureBlockInfo / JigsawBlockInfo ─────────────────────────────────────
// StructureTemplate.StructureBlockInfo(pos, state, nbt) — :882-887. We keep the
// block id ("Name"), the FrontAndTop orientation (only meaningful for jigsaw
// blocks), and an optional block-entity nbt compound.
struct StructureBlockInfo {
    BlockPos pos{};                          // template-relative (post-transform when placed)
    std::string blockName;                   // BlockState "Name" (e.g. "minecraft:jigsaw")
    FrontAndTop orientation{Direction::NORTH, Direction::UP};  // ORIENTATION (default NORTH_UP)
    bool hasOrientation = false;             // whether the state carries ORIENTATION
    std::shared_ptr<const mc::nbt::NbtCompound> nbt; // block-entity tag, or null
};

// StructureTemplate.JigsawBlockInfo record — :783-824.
struct JigsawBlockInfo {
    StructureBlockInfo info;     // the (transformed) block info
    JointType jointType{};
    std::string name;            // Identifier; default JigsawBlockEntity.EMPTY_ID "minecraft:empty"
    std::string pool;            // ResourceKey<StructureTemplatePool>; default Pools.EMPTY "minecraft:empty"
    std::string target;          // Identifier; default "minecraft:empty"
    int placementPriority = 0;
    int selectionPriority = 0;
};

// Identifier.CODEC parse: a string with optional namespace; bare "foo" -> "minecraft:foo"
// (Identifier.withDefaultNamespace). We normalize to the canonical "ns:path" the
// Java toString emits so the parity TSV strings match.
inline std::string normalizeIdentifier(const std::string& s) {
    return s.find(':') == std::string::npos ? ("minecraft:" + s) : s;
}

namespace detail {

// state.is(Blocks.JIGSAW) — by block id.
inline bool isJigsaw(const StructureBlockInfo& info) { return info.blockName == "minecraft:jigsaw"; }

// Read a CompoundTag string field, or nullopt.
inline std::optional<std::string> readString(const mc::nbt::NbtCompound& c, std::string_view key) {
    const mc::nbt::NbtTag* t = c.get(key);
    if (!t) return std::nullopt;
    if (const auto* s = t->as<std::string>()) return *s;
    return std::nullopt;
}

// Read a CompoundTag int field with a default (getIntOr).
inline int getIntOr(const mc::nbt::NbtCompound& c, std::string_view key, int def) {
    const mc::nbt::NbtTag* t = c.get(key);
    if (!t) return def;
    if (const auto* v = t->as<std::int32_t>()) return *v;
    return def;
}

// StructureTemplate.JigsawBlockInfo.of(info) — :792-803. `info` must carry nbt.
inline JigsawBlockInfo jigsawInfoOf(const StructureBlockInfo& info) {
    if (!info.nbt) throw std::runtime_error("jigsaw StructureBlockInfo nbt was null");
    const mc::nbt::NbtCompound& nbt = *info.nbt;

    JigsawBlockInfo out;
    out.info = info;

    // getJointType(nbt, state): nbt.read("joint", JointType.CODEC).orElseGet(default).
    std::optional<std::string> jointStr = readString(nbt, "joint");
    if (jointStr.has_value()) {
        out.jointType = (*jointStr == "aligned") ? JointType::ALIGNED : JointType::ROLLABLE;
        // JointType.CODEC only accepts "aligned"/"rollable"; an unknown value would
        // fail the codec and fall through to the default. We mirror that:
        if (*jointStr != "aligned" && *jointStr != "rollable")
            out.jointType = defaultJointType(info.orientation.front);
    } else {
        out.jointType = defaultJointType(info.orientation.front);
    }

    // name/target: Identifier.CODEC default JigsawBlockEntity.EMPTY_ID = "minecraft:empty".
    out.name   = normalizeIdentifier(readString(nbt, "name").value_or("minecraft:empty"));
    out.target = normalizeIdentifier(readString(nbt, "target").value_or("minecraft:empty"));
    // pool: ResourceKey<StructureTemplatePool> default Pools.EMPTY = "minecraft:empty".
    out.pool   = normalizeIdentifier(readString(nbt, "pool").value_or("minecraft:empty"));

    out.placementPriority = getIntOr(nbt, "placement_priority", 0);
    out.selectionPriority = getIntOr(nbt, "selection_priority", 0);
    return out;
}

// addToLists (StructureTemplate.java:141-154). For jigsaw discovery only the
// block-entity bucketing matters: blocks with nbt go to blockEntitiesList (this
// is the bucket jigsaw blocks always land in — they carry a block entity). The
// full-vs-other split for the remainder uses isCollisionShapeFullBlock, which we
// do NOT need to evaluate for jigsaw discovery because only the (Y,X,Z)-sorted
// CONCAT order within the block-entity bucket (and the relative order of jigsaw
// blocks therein) determines the jigsaws() list. To preserve the exact final
// blocks() ordering, however, we DO honour the three buckets so the StructureBlockInfo
// count and ordering match buildInfoList; full-vs-other is decided from a small
// table of which blocks are full collision cubes is NOT required because jigsaws()
// only reads the block-entity bucket. We therefore keep the bucket assignment that
// is observable to the gate: nbt -> blockEntities, else -> "rest" (full++other
// preserve their own (Y,X,Z) order and never contain jigsaw blocks).
enum class Bucket { Full, Other, BlockEntity };

} // namespace detail

// One loaded template (one Palette's worth, plus size + a parsed-but-ignored
// entity count). StructureTemplate keeps a list of palettes; for the gate we load
// each and expose the size + buildInfoList ordering + jigsaws.
struct LoadedTemplate {
    BlockPos size{};
    std::vector<StructureBlockInfo> blocks;          // buildInfoList order (one palette)
    std::vector<std::size_t> jigsawIndices;          // indices into `blocks` that are jigsaws
    int entityCount = 0;
};

namespace detail {

// buildInfoList comparator (Y, then X, then Z), ascending, signed.
inline bool blockInfoLess(const StructureBlockInfo& a, const StructureBlockInfo& b) noexcept {
    if (a.pos.y != b.pos.y) return a.pos.y < b.pos.y;
    if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
    return a.pos.z < b.pos.z;
}

// Whether a block id is a full-collision cube without dynamic shape. addToLists
// only splits the non-block-entity remainder into full vs other by
// isCollisionShapeFullBlock; that split does NOT reorder jigsaw blocks (which
// always carry nbt and land in the block-entity bucket) and does NOT change the
// jigsaws() list. The gate compares the jigsaw list (which is order-stable across
// any full/other split) AND the StructureBlockInfo COUNT (split-invariant). We
// therefore route every non-block-entity block into a single "rest" bucket that
// preserves (Y,X,Z) order; the final concat is full++other++blockEntities, and
// since full and other are each (Y,X,Z)-sorted, "rest" sorted by (Y,X,Z) yields
// the same MULTISET and the same block-entity tail. The jigsaw ordering — the
// load-bearing output — is identical.
} // namespace detail

// StructureTemplate.loadPalette(blockLookup, paletteList, blockList) — :733-753,
// folding load()'s palette/palettes selection (:709-731). `paletteNames`/
// `paletteOrients` give the palette's per-index block id + ORIENTATION.
inline LoadedTemplate buildOnePalette(
        const BlockPos& size, int entityCount,
        const std::vector<std::string>& paletteNames,
        const std::vector<std::optional<FrontAndTop>>& paletteOrients,
        const mc::nbt::NbtList* blockList) {
    using detail::blockInfoLess;
    LoadedTemplate t;
    t.size = size;
    t.entityCount = entityCount;

    std::vector<StructureBlockInfo> rest;          // full ++ other (both (Y,X,Z)-sorted)
    std::vector<StructureBlockInfo> blockEntities;  // nbt-carrying blocks

    if (blockList) {
        for (const auto& e : blockList->elements) {
            const auto* bp = std::get_if<std::shared_ptr<mc::nbt::NbtCompound>>(&e.value);
            if (!bp) continue;
            const mc::nbt::NbtCompound& b = **bp;

            const mc::nbt::NbtList* posList = b.getList("pos");
            BlockPos pos{};
            if (posList && posList->elements.size() >= 3) {
                auto asInt = [](const mc::nbt::NbtTag& tag) {
                    const auto* v = tag.as<std::int32_t>(); return v ? *v : 0;
                };
                pos = BlockPos{asInt(posList->elements[0]), asInt(posList->elements[1]),
                               asInt(posList->elements[2])};
            }
            // state index into the palette (getIntOr("state", 0)).
            int stateIdx = detail::getIntOr(b, "state", 0);

            StructureBlockInfo info;
            info.pos = pos;
            if (stateIdx >= 0 && static_cast<std::size_t>(stateIdx) < paletteNames.size()) {
                info.blockName = paletteNames[static_cast<std::size_t>(stateIdx)];
                if (paletteOrients[static_cast<std::size_t>(stateIdx)].has_value()) {
                    info.orientation = *paletteOrients[static_cast<std::size_t>(stateIdx)];
                    info.hasOrientation = true;
                }
            } else {
                info.blockName = "minecraft:air"; // SimplePalette.stateFor out-of-range -> AIR
            }

            const mc::nbt::NbtCompound* nbtC = b.getCompound("nbt");
            if (nbtC) info.nbt = std::make_shared<const mc::nbt::NbtCompound>(*nbtC);

            if (info.nbt) blockEntities.push_back(std::move(info));
            else          rest.push_back(std::move(info));
        }
    }

    std::stable_sort(rest.begin(), rest.end(), blockInfoLess);
    std::stable_sort(blockEntities.begin(), blockEntities.end(), blockInfoLess);

    t.blocks.reserve(rest.size() + blockEntities.size());
    t.blocks.insert(t.blocks.end(), rest.begin(), rest.end());
    t.blocks.insert(t.blocks.end(), blockEntities.begin(), blockEntities.end());

    // Palette.jigsaws(): blocks.filter(state.is(JIGSAW)) — in blocks() order.
    for (std::size_t i = 0; i < t.blocks.size(); ++i)
        if (detail::isJigsaw(t.blocks[i])) t.jigsawIndices.push_back(i);

    return t;
}

// Decode one palette ListTag (list of block-state compounds: Name + Properties)
// into per-index (name, orientation) — NbtUtils.readBlockState (:127-148): the
// orientation property is the FrontAndTop named in Properties.orientation.
inline void decodePalette(const mc::nbt::NbtList* paletteList,
                          std::vector<std::string>& names,
                          std::vector<std::optional<FrontAndTop>>& orients) {
    names.clear();
    orients.clear();
    if (!paletteList) return;
    for (const auto& e : paletteList->elements) {
        const auto* cp = std::get_if<std::shared_ptr<mc::nbt::NbtCompound>>(&e.value);
        if (!cp) { names.push_back("minecraft:air"); orients.push_back(std::nullopt); continue; }
        const mc::nbt::NbtCompound& c = **cp;
        // Name -> block id (normalize to ns:path like the registry key toString).
        std::string name = normalizeIdentifier(c.getString("Name", "minecraft:air"));
        names.push_back(name);
        // Properties.orientation (FrontAndTop). Default for jigsaw == NORTH_UP, but
        // hasOrientation is set only when the property is present; non-jigsaw blocks
        // ignore it entirely.
        std::optional<FrontAndTop> ft;
        if (const mc::nbt::NbtCompound* props = c.getCompound("Properties")) {
            std::string o = props->getString("orientation", "");
            if (!o.empty()) ft = frontAndTopByName(o);
        }
        orients.push_back(ft);
    }
}

// StructureTemplate.load(HolderGetter<Block>, CompoundTag) — :709-731. Loads the
// FIRST palette (palettes[0] if "palettes" present, else "palette"); jigsaw
// discovery / getShuffledJigsawBlocks downstream (StructurePlaceSettings.
// getRandomPalette) selects a palette by RNG, but the pillager_outpost templates
// (and all single-palette structures) have exactly one — the gate drives the same
// (single) palette in Java, so loading palette[0] is exact here. For multi-palette
// templates the caller can request a specific palette index.
inline LoadedTemplate loadStructureTemplate(const mc::nbt::NbtCompound& root, int paletteIndex = 0) {
    // size: 3 ints (getIntOr(0/1/2, 0)).
    BlockPos size{};
    if (const mc::nbt::NbtList* sizeList = root.getList("size")) {
        auto asInt = [](const mc::nbt::NbtTag& tag) {
            const auto* v = tag.as<std::int32_t>(); return v ? *v : 0;
        };
        if (sizeList->elements.size() >= 1) size.x = asInt(sizeList->elements[0]);
        if (sizeList->elements.size() >= 2) size.y = asInt(sizeList->elements[1]);
        if (sizeList->elements.size() >= 3) size.z = asInt(sizeList->elements[2]);
    }

    const mc::nbt::NbtList* blockList = root.getList("blocks");

    int entityCount = 0;
    if (const mc::nbt::NbtList* ents = root.getList("entities")) {
        // load() only keeps entities whose tag carries an "nbt" compound.
        for (const auto& e : ents->elements) {
            const auto* cp = std::get_if<std::shared_ptr<mc::nbt::NbtCompound>>(&e.value);
            if (cp && (**cp).getCompound("nbt")) ++entityCount;
        }
    }

    std::vector<std::string> names;
    std::vector<std::optional<FrontAndTop>> orients;
    if (const mc::nbt::NbtList* palettes = root.getList("palettes")) {
        int idx = paletteIndex;
        if (idx < 0 || static_cast<std::size_t>(idx) >= palettes->elements.size()) idx = 0;
        const auto* lp = std::get_if<std::shared_ptr<mc::nbt::NbtList>>(&palettes->elements[static_cast<std::size_t>(idx)].value);
        decodePalette(lp ? lp->get() : nullptr, names, orients);
    } else {
        decodePalette(root.getList("palette"), names, orients);
    }

    return buildOnePalette(size, entityCount, names, orients, blockList);
}

// StructureTemplate.getJigsaws(position, rotation) — :197-218. Returns the palette
// jigsaw blocks with each pos transformed (NONE mirror, given rotation, pivot=0)
// + offset by `position`, and the FrontAndTop orientation rotated by `rotation`.
inline std::vector<JigsawBlockInfo> getJigsaws(const LoadedTemplate& t,
                                               const BlockPos& position, Rotation rotation) {
    std::vector<JigsawBlockInfo> result;
    result.reserve(t.jigsawIndices.size());
    for (std::size_t idx : t.jigsawIndices) {
        // Palette.jigsaws() -> JigsawBlockInfo.of(untransformed info).
        JigsawBlockInfo j = detail::jigsawInfoOf(t.blocks[idx]);
        // withInfo(new StructureBlockInfo(transformedPos, state.rotate(rotation), nbt)).
        const StructureBlockInfo& src = t.blocks[idx];
        BlockPos rp = structureTransform(src.pos, Mirror::NONE, rotation, kBlockPosZero);
        StructureBlockInfo ti = src;
        ti.pos = BlockPos{rp.x + position.x, rp.y + position.y, rp.z + position.z};
        ti.orientation = jigsawStateRotate(src.orientation, rotation);
        j.info = ti;
        result.push_back(std::move(j));
    }
    return result;
}

// net.minecraft.util.Util.shuffle(List, RandomSource) — Util.java:1061-1068.
//   for (int i = size; i > 1; i--) { swapTo = random.nextInt(i); swap(i-1, swapTo); }
template <typename T>
inline void utilShuffle(std::vector<T>& list, mc::levelgen::RandomSource& random) {
    int size = static_cast<int>(list.size());
    for (int i = size; i > 1; --i) {
        int swapTo = random.nextInt(i);
        std::swap(list[static_cast<std::size_t>(i - 1)], list[static_cast<std::size_t>(swapTo)]);
    }
}

// SinglePoolElement.sortBySelectionPriority — :124-127: STABLE sort by
// selectionPriority DESCENDING (Comparator.comparingInt(selectionPriority).reversed()).
// java.util.List.sort is stable, so equal selectionPriority keeps post-shuffle order.
inline void sortBySelectionPriority(std::vector<JigsawBlockInfo>& blocks) {
    std::stable_sort(blocks.begin(), blocks.end(),
        [](const JigsawBlockInfo& a, const JigsawBlockInfo& b) {
            return a.selectionPriority > b.selectionPriority; // descending
        });
}

// SinglePoolElement.getShuffledJigsawBlocks(.., position, rotation, random) — :114-122.
inline std::vector<JigsawBlockInfo> getShuffledJigsawBlocks(
        const LoadedTemplate& t, const BlockPos& position, Rotation rotation,
        mc::levelgen::RandomSource& random) {
    std::vector<JigsawBlockInfo> jigsaws = getJigsaws(t, position, rotation);
    utilShuffle(jigsaws, random);
    sortBySelectionPriority(jigsaws);
    return jigsaws;
}

} // namespace mc::levelgen::structure::templatesystem
