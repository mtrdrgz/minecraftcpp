// 1:1 port of the two pure enums declared inside net.minecraft.world.level.ClipContext
// (26.1.2/src/net/minecraft/world/level/ClipContext.java):
//
//   public enum Block implements ClipContext.ShapeGetter {
//      COLLIDER, OUTLINE, VISUAL, FALLDAMAGE_RESETTING;
//   }
//   public enum Fluid {
//      NONE, SOURCE_ONLY, ANY, WATER;
//   }
//
// Only the enum SHAPE is portable bit-exact here: the declaration order
// (== ordinal()) and the constant name() of every constant. Neither enum
// implements StringRepresentable, so there is no getSerializedName(); the only
// pure accessors a plain Java enum exposes are ordinal() and name().
//
// The behavioural payloads are deliberately NOT ported:
//   * Block.get(state, level, pos, context) dispatches to
//     BlockStateBase::getCollisionShape / getShape / getVisualShape, or (for
//     FALLDAMAGE_RESETTING) reads BlockTags.FALL_DAMAGE_RESETTING, the
//     EntityCollisionContext entity, Blocks.END_GATEWAY/END_PORTAL/NETHER_PORTAL
//     and a ServerLevel game rule — all of which require live world / registry /
//     tag / collision-context infrastructure that is out of scope for this gate.
//   * Fluid.canPick(fluidState) calls FluidState::isSource / isEmpty /
//     is(FluidTags.WATER), which needs a live FluidState + tag system.
// Those getShape/canPick lookups are intentionally skipped (see test notes).

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace mc::world::level::clipcontext {

// Declaration order MUST equal ClipContext.Block's source order (== ordinal()).
// ClipContext.java lines 56-82.
enum class Block : int {
    COLLIDER = 0,
    OUTLINE = 1,
    VISUAL = 2,
    FALLDAMAGE_RESETTING = 3,
};

// Declaration order MUST equal ClipContext.Fluid's source order (== ordinal()).
// ClipContext.java lines 96-100.
enum class Fluid : int {
    NONE = 0,
    SOURCE_ONLY = 1,
    ANY = 2,
    WATER = 3,
};

// name() of every ClipContext.Block constant, indexed by ordinal.
inline const std::vector<std::string>& blockNames() {
    static const std::vector<std::string> kNames = {
        "COLLIDER",
        "OUTLINE",
        "VISUAL",
        "FALLDAMAGE_RESETTING",
    };
    return kNames;
}

// name() of every ClipContext.Fluid constant, indexed by ordinal.
inline const std::vector<std::string>& fluidNames() {
    static const std::vector<std::string> kNames = {
        "NONE",
        "SOURCE_ONLY",
        "ANY",
        "WATER",
    };
    return kNames;
}

// ordinal() of a Block constant.
inline int ordinal(Block b) { return static_cast<int>(b); }
// ordinal() of a Fluid constant.
inline int ordinal(Fluid f) { return static_cast<int>(f); }

// name() lookup by ordinal; empty string if out of range.
inline const std::string& blockName(int ordinal) {
    static const std::string kEmpty;
    const auto& n = blockNames();
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= n.size()) return kEmpty;
    return n[static_cast<std::size_t>(ordinal)];
}
inline const std::string& fluidName(int ordinal) {
    static const std::string kEmpty;
    const auto& n = fluidNames();
    if (ordinal < 0 || static_cast<std::size_t>(ordinal) >= n.size()) return kEmpty;
    return n[static_cast<std::size_t>(ordinal)];
}

}  // namespace mc::world::level::clipcontext
