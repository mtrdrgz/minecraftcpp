#pragma once

// ── net.minecraft.world.level.block.Mirror — StringRepresentable id strings ──
//
// The Mirror ENUM ITSELF (ordinals + the three pure methods mirror(int,int),
// getRotation(Direction), mirror(Direction)) is already ported and certified
// byte-exact in
//   world/level/levelgen/structure/templatesystem/StructureTransforms.h
// (gate: structure_transform_parity). This tiny companion header adds ONLY the
// one piece that header does not carry — the StringRepresentable serialized
// names — so the dedicated mirror_parity gate can also certify
// getSerializedName(). Nothing here is invented: the strings are copied verbatim
// from the enum-constant declarations.
//
//   Mirror.NONE       -> "none"        (Mirror.java:11)
//   Mirror.LEFT_RIGHT -> "left_right"  (Mirror.java:12)
//   Mirror.FRONT_BACK -> "front_back"  (Mirror.java:13)
//   getSerializedName() returns this.id (Mirror.java:62-65).
//
// The C++ name() (== Java Enum.name()) is the identifier verbatim.

#include "../levelgen/structure/templatesystem/StructureTransforms.h"

namespace mc::block {

using mc::levelgen::structure::Mirror;

// Mirror.getSerializedName() — returns the `id` ctor arg (Mirror.java:11-13,63).
constexpr const char* mirrorGetSerializedName(Mirror m) noexcept {
    switch (m) {
        case Mirror::NONE:       return "none";
        case Mirror::LEFT_RIGHT: return "left_right";
        case Mirror::FRONT_BACK: return "front_back";
    }
    return "";
}

// Java Enum.name() — the declared constant identifier (Mirror.java:11-13).
constexpr const char* mirrorName(Mirror m) noexcept {
    switch (m) {
        case Mirror::NONE:       return "NONE";
        case Mirror::LEFT_RIGHT: return "LEFT_RIGHT";
        case Mirror::FRONT_BACK: return "FRONT_BACK";
    }
    return "";
}

} // namespace mc::block
