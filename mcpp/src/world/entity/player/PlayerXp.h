// 1:1 port of two pure XP integer formulas from Minecraft 26.1.2:
//
//   net.minecraft.world.entity.player.Player.getXpNeededForNextLevel()
//     (instance method; reads this.experienceLevel — here taken as a parameter)
//   net.minecraft.world.entity.ExperienceOrb.getExperienceValue(int maxValue)  [static]
//
// Both are pure two's-complement int math with no un-ported dependencies.
// Verified bit-for-bit against the real classes by player_xp_parity
// (tools/PlayerXpParity.java).
#pragma once

namespace mc::world::entity::player {

// Player.java (26.1.2), lines 1553-1559. The instance field this.experienceLevel
// is the only state read, so it is lifted to the `experienceLevel` parameter.
//
//   public int getXpNeededForNextLevel() {
//      if (this.experienceLevel >= 30) {
//         return 112 + (this.experienceLevel - 30) * 9;
//      } else {
//         return this.experienceLevel >= 15 ? 37 + (this.experienceLevel - 15) * 5 : 7 + this.experienceLevel * 2;
//      }
//   }
inline int getXpNeededForNextLevel(int experienceLevel) {
    if (experienceLevel >= 30) {
        return 112 + (experienceLevel - 30) * 9;
    } else {
        return experienceLevel >= 15 ? 37 + (experienceLevel - 15) * 5 : 7 + experienceLevel * 2;
    }
}

// ExperienceOrb.java (26.1.2), lines 346-368. Verbatim static body.
//
//   public static int getExperienceValue(final int maxValue) {
//      if (maxValue >= 2477) { return 2477; }
//      else if (maxValue >= 1237) { return 1237; }
//      ... down to ...
//      else { return maxValue >= 3 ? 3 : 1; }
//   }
inline int getExperienceValue(int maxValue) {
    if (maxValue >= 2477) {
        return 2477;
    } else if (maxValue >= 1237) {
        return 1237;
    } else if (maxValue >= 617) {
        return 617;
    } else if (maxValue >= 307) {
        return 307;
    } else if (maxValue >= 149) {
        return 149;
    } else if (maxValue >= 73) {
        return 73;
    } else if (maxValue >= 37) {
        return 37;
    } else if (maxValue >= 17) {
        return 17;
    } else if (maxValue >= 7) {
        return 7;
    } else {
        return maxValue >= 3 ? 3 : 1;
    }
}

} // namespace mc::world::entity::player
