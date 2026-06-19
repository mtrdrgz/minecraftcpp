// 1:1 port of the deterministic, RNG-driven shot helpers of
// net.minecraft.world.item.CrossbowItem (Minecraft 26.1.2).
//
// Source: 26.1.2/src/net/minecraft/world/item/CrossbowItem.java
//
// What this covers (the pure, world-free pieces):
//   * getShootingPower(projectiles)  -> FIREWORK_POWER (1.6F) if the charged
//     projectiles contain a firework rocket, else ARROW_POWER (3.15F).
//   * getShotPitch(random, index)    -> 1.0F for the first projectile (index 0),
//     otherwise getRandomShotPitch((index & 1) == 1, random).
//   * getRandomShotPitch(highPitch, random) ->
//         1.0F / (random.nextFloat() * 0.5F + 1.8F) + rangeDecider
//     with rangeDecider = highPitch ? 0.63F : 0.43F.
//
// All arithmetic is single-precision float, exactly as in Java (the constants
// 0.5F / 1.8F / 0.63F / 0.43F and the 1.0F numerator are float literals, so the
// whole expression stays in float). nextFloat() consumes exactly one RNG draw
// (next(24)), and only on the getRandomShotPitch path (index != 0). index 0 is a
// pure constant and draws nothing.
//
// getShootingPower is RNG-free; the C++ side takes a bool (containsFirework)
// because the only thing the real method inspects is whether the charged set
// contains Items.FIREWORK_ROCKET.
#pragma once

#include <cstdint>

#include "world/level/levelgen/RandomSource.h"

namespace mc::world::item {

// CrossbowItem private constants (FIREWORK_POWER, ARROW_POWER).
inline constexpr float CROSSBOW_FIREWORK_POWER = 1.6F;
inline constexpr float CROSSBOW_ARROW_POWER = 3.15F;

// private static float getShootingPower(ChargedProjectiles projectiles) {
//    return projectiles.contains(Items.FIREWORK_ROCKET) ? 1.6F : 3.15F;
// }
inline constexpr float getShootingPower(bool containsFirework) {
   return containsFirework ? CROSSBOW_FIREWORK_POWER : CROSSBOW_ARROW_POWER;
}

// private static float getRandomShotPitch(boolean highPitch, RandomSource random) {
//    float rangeDecider = highPitch ? 0.63F : 0.43F;
//    return 1.0F / (random.nextFloat() * 0.5F + 1.8F) + rangeDecider;
// }
inline float getRandomShotPitch(bool highPitch, mc::levelgen::RandomSource& random) {
   const float rangeDecider = highPitch ? 0.63F : 0.43F;
   return 1.0F / (random.nextFloat() * 0.5F + 1.8F) + rangeDecider;
}

// private static float getShotPitch(RandomSource random, int index) {
//    return index == 0 ? 1.0F : getRandomShotPitch((index & 1) == 1, random);
// }
inline float getShotPitch(mc::levelgen::RandomSource& random, std::int32_t index) {
   return index == 0 ? 1.0F : getRandomShotPitch((index & 1) == 1, random);
}

}  // namespace mc::world::item
