// 1:1 C++ port of net.minecraft.world.entity.ai.memory.MemoryStatus
// (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/entity/ai/memory/MemoryStatus.java):
//
//   package net.minecraft.world.entity.ai.memory;
//
//   public enum MemoryStatus {
//      VALUE_PRESENT,
//      VALUE_ABSENT,
//      REGISTERED;
//   }
//
// A plain Java enum with no fields/constructor. The only observable, ported
// state is the declaration order (= ordinal()) and the constant name (= name()).
// We mirror that here as a scoped enum whose underlying integer values are the
// Java ordinals, plus a name() helper that returns the verbatim Java constant
// names. The parity gate (MemoryStatusParityTest) checks each ordinal+name
// against the REAL net.minecraft enum.

#ifndef MCPP_WORLD_ENTITY_AI_MEMORY_MEMORYSTATUS_H
#define MCPP_WORLD_ENTITY_AI_MEMORY_MEMORYSTATUS_H

#include <array>
#include <cstddef>
#include <string_view>

namespace mc::world::entity::ai::memory {

// Underlying values are the Java ordinals (declaration order, 0-based).
enum class MemoryStatus : int {
    VALUE_PRESENT = 0,
    VALUE_ABSENT = 1,
    REGISTERED = 2,
};

// Java enum's implicit values() array, in declaration order.
inline constexpr std::array<MemoryStatus, 3> MEMORY_STATUS_VALUES = {
    MemoryStatus::VALUE_PRESENT,
    MemoryStatus::VALUE_ABSENT,
    MemoryStatus::REGISTERED,
};

// MemoryStatus.ordinal() — the position in the declaration order.
inline constexpr int ordinal(MemoryStatus s) { return static_cast<int>(s); }

// MemoryStatus.name() — the verbatim Java constant name.
inline constexpr std::string_view name(MemoryStatus s) {
    switch (s) {
        case MemoryStatus::VALUE_PRESENT: return "VALUE_PRESENT";
        case MemoryStatus::VALUE_ABSENT:  return "VALUE_ABSENT";
        case MemoryStatus::REGISTERED:    return "REGISTERED";
    }
    return ""; // unreachable for the three defined constants
}

} // namespace mc::world::entity::ai::memory

#endif // MCPP_WORLD_ENTITY_AI_MEMORY_MEMORYSTATUS_H
