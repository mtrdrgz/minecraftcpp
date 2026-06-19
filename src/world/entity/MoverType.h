// 1:1 C++ port of net.minecraft.world.entity.MoverType (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/entity/MoverType.java):
//
//   public enum MoverType {
//      SELF,
//      PLAYER,
//      PISTON,
//      SHULKER_BOX,
//      SHULKER;
//   }
//
// A plain Java enum with no fields/constructor. The only observable, ported
// state is the declaration order (= ordinal()) and the constant name (= name()).
// We mirror that here as a scoped enum whose underlying integer values are the
// Java ordinals, plus a name() helper that returns the verbatim Java constant
// names. The parity gate (MoverTypeParityTest) checks each ordinal+name against
// the REAL net.minecraft enum.

#ifndef MCPP_WORLD_ENTITY_MOVERTYPE_H
#define MCPP_WORLD_ENTITY_MOVERTYPE_H

#include <array>
#include <cstddef>
#include <string_view>

namespace mc {

// Underlying values are the Java ordinals (declaration order, 0-based).
enum class MoverType : int {
    SELF = 0,
    PLAYER = 1,
    PISTON = 2,
    SHULKER_BOX = 3,
    SHULKER = 4,
};

// Java enum's implicit values() array, in declaration order.
inline constexpr std::array<MoverType, 5> MOVER_TYPE_VALUES = {
    MoverType::SELF,
    MoverType::PLAYER,
    MoverType::PISTON,
    MoverType::SHULKER_BOX,
    MoverType::SHULKER,
};

// MoverType.ordinal() — the position in the declaration order.
inline constexpr int ordinal(MoverType t) { return static_cast<int>(t); }

// MoverType.name() — the verbatim Java constant name.
inline constexpr std::string_view name(MoverType t) {
    switch (t) {
        case MoverType::SELF:        return "SELF";
        case MoverType::PLAYER:      return "PLAYER";
        case MoverType::PISTON:      return "PISTON";
        case MoverType::SHULKER_BOX: return "SHULKER_BOX";
        case MoverType::SHULKER:     return "SHULKER";
    }
    return ""; // unreachable for the five defined constants
}

} // namespace mc

#endif // MCPP_WORLD_ENTITY_MOVERTYPE_H
