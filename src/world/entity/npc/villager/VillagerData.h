// 1:1 C++ port of net.minecraft.world.entity.npc.villager.VillagerData
// (Minecraft 26.1.2) — the pure, world-free leveling/XP-threshold helpers.
//
// Java source: 26.1.2/src/net/minecraft/world/entity/npc/villager/VillagerData.java
//
// VillagerData is a record (Holder<VillagerType>, Holder<VillagerProfession>,
// int level). The registry-Holder fields are render/registry state and are NOT
// modeled here; what IS modeled is the fully self-contained gameplay arithmetic
// that drives villager trade-tier progression:
//
//   * the canonical-constructor level clamp        level = Math.max(1, level)
//   * static int getMinXpPerLevel(int level)
//   * static int getMaxXpPerLevel(int level)
//   * static boolean canLevelUp(int currentLevel)
//
// These are pure integer functions of `level` only: no world/level access, no
// entity state, no registry/datapack lookups, no GL. Villager.shouldIncreaseLevel
// (Villager.java) is literally
//     VillagerData.canLevelUp(level) && villagerXp >= VillagerData.getMaxXpPerLevel(level)
// so this is the exact gating math that decides when a villager's profession
// level advances.
//
// Verbatim constants from the Java:
//   MIN_VILLAGER_LEVEL = 1
//   MAX_VILLAGER_LEVEL = 5
//   NEXT_LEVEL_XP_THRESHOLDS = { 0, 10, 70, 150, 250 }   (private, length 5)
//
//   public VillagerData { level = Math.max(1, level); }   // compact ctor clamp
//
//   getMinXpPerLevel(level) = canLevelUp(level) ? THRESHOLDS[level - 1] : 0
//   getMaxXpPerLevel(level) = canLevelUp(level) ? THRESHOLDS[level]     : 0
//   canLevelUp(level)       = level >= 1 && level < 5
//
// 1:1 traps reproduced verbatim:
//   * The `canLevelUp` gate is what keeps the array accesses in bounds. canLevelUp
//     is true only for level in {1,2,3,4}. getMinXpPerLevel indexes [level-1]
//     (0..3) and getMaxXpPerLevel indexes [level] (1..4); both are valid ONLY
//     because canLevelUp already excluded level<=0 and level>=5. For any level
//     outside {1,2,3,4} BOTH functions short-circuit to 0 *before* indexing — so
//     no clamp, no wrap, no out-of-bounds read ever happens. We replicate the
//     short-circuit exactly (the array is never indexed unless canLevelUp holds).
//   * The constructor clamp is Math.max(1, level): only the lower bound is
//     clamped to MIN_VILLAGER_LEVEL=1; there is NO upper clamp to
//     MAX_VILLAGER_LEVEL here, so e.g. clampLevel(99) == 99.
//
// Nothing here is invented; every value/formula comes from the Java above.

#ifndef MCPP_WORLD_ENTITY_NPC_VILLAGER_VILLAGERDATA_H
#define MCPP_WORLD_ENTITY_NPC_VILLAGER_VILLAGERDATA_H

#include <array>
#include <cstdint>

namespace mc::world::entity::npc::villager {

// public static final int MIN_VILLAGER_LEVEL = 1;
inline constexpr int32_t MIN_VILLAGER_LEVEL = 1;
// public static final int MAX_VILLAGER_LEVEL = 5;
inline constexpr int32_t MAX_VILLAGER_LEVEL = 5;

// private static final int[] NEXT_LEVEL_XP_THRESHOLDS = new int[]{0, 10, 70, 150, 250};
inline constexpr std::array<int32_t, 5> NEXT_LEVEL_XP_THRESHOLDS = {
    0, 10, 70, 150, 250,
};

// VillagerData.canLevelUp(int currentLevel):
//   return currentLevel >= 1 && currentLevel < 5;
inline bool canLevelUp(int32_t currentLevel) {
    return currentLevel >= 1 && currentLevel < 5;
}

// VillagerData.getMinXpPerLevel(int level):
//   return canLevelUp(level) ? NEXT_LEVEL_XP_THRESHOLDS[level - 1] : 0;
// The ternary short-circuits: NEXT_LEVEL_XP_THRESHOLDS is indexed ONLY when
// canLevelUp(level) holds (level in 1..4), so [level-1] is always 0..3 (in range).
inline int32_t getMinXpPerLevel(int32_t level) {
    return canLevelUp(level)
               ? NEXT_LEVEL_XP_THRESHOLDS[static_cast<size_t>(level - 1)]
               : 0;
}

// VillagerData.getMaxXpPerLevel(int level):
//   return canLevelUp(level) ? NEXT_LEVEL_XP_THRESHOLDS[level] : 0;
// Indexed ONLY when canLevelUp(level) holds (level in 1..4), so [level] is
// always 1..4 (in range).
inline int32_t getMaxXpPerLevel(int32_t level) {
    return canLevelUp(level)
               ? NEXT_LEVEL_XP_THRESHOLDS[static_cast<size_t>(level)]
               : 0;
}

// The compact (canonical) constructor body:  public VillagerData { level = Math.max(1, level); }
// Only the field value matters for parity; this isolates the clamp so it can be
// driven without constructing the registry-Holder fields. Math.max(1, level):
// lower-bounds to 1, no upper clamp.
inline int32_t clampLevel(int32_t level) {
    return level > 1 ? level : 1;  // Math.max(1, level)
}

} // namespace mc::world::entity::npc::villager

#endif  // MCPP_WORLD_ENTITY_NPC_VILLAGER_VILLAGERDATA_H
