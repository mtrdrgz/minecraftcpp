// 1:1 port of net.minecraft.world.level.material.PushReaction (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/level/material/PushReaction.java):
//
//   public enum PushReaction {
//      NORMAL,
//      DESTROY,
//      BLOCK,
//      IGNORE,
//      PUSH_ONLY;
//   }
//
// A bare enum with no fields or methods. The ONLY observable, port-relevant
// contract is the declaration order (== Enum.ordinal()) and each constant's
// name (== Enum.name()). Both are reproduced here VERBATIM in the same order, so
// the underlying ordinals (0..4) match the real enum bit-for-bit.
//
// Certified by tools/PushReactionParity.java + PushReactionParityTest.cpp.

#ifndef MCPP_WORLD_LEVEL_MATERIAL_PUSHREACTION_H
#define MCPP_WORLD_LEVEL_MATERIAL_PUSHREACTION_H

#include <cstddef>
#include <string_view>

namespace mc::world::level::material {

// Order is load-bearing: each constant's position is its Java Enum.ordinal().
enum class PushReaction {
    NORMAL = 0,
    DESTROY = 1,
    BLOCK = 2,
    IGNORE = 3,
    PUSH_ONLY = 4,
};

// Number of declared constants (Java PushReaction.values().length).
inline constexpr std::size_t PUSH_REACTION_COUNT = 5;

// Enum.ordinal(): the zero-based declaration index.
inline constexpr int pushReactionOrdinal(PushReaction r) {
    return static_cast<int>(r);
}

// Enum.name(): the constant identifier exactly as declared in Java.
inline constexpr std::string_view pushReactionName(PushReaction r) {
    switch (r) {
        case PushReaction::NORMAL:    return "NORMAL";
        case PushReaction::DESTROY:   return "DESTROY";
        case PushReaction::BLOCK:     return "BLOCK";
        case PushReaction::IGNORE:    return "IGNORE";
        case PushReaction::PUSH_ONLY: return "PUSH_ONLY";
    }
    return "";  // unreachable for valid inputs
}

// PushReaction.values()[ordinal] — index back to the constant. Mirrors the
// implicit array Java's compiler synthesizes; order matches declaration.
inline constexpr PushReaction pushReactionByOrdinal(int ordinal) {
    return static_cast<PushReaction>(ordinal);
}

}  // namespace mc::world::level::material

#endif  // MCPP_WORLD_LEVEL_MATERIAL_PUSHREACTION_H
