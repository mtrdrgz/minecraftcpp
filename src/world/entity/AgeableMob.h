// 1:1 port of the pure, self-contained arithmetic of
//   net.minecraft.world.entity.AgeableMob (Minecraft 26.1.2).
//
// AgeableMob itself is an abstract, world-coupled entity, but it exposes one
// genuinely pure static helper whose body reads no entity/world/registry state:
//
//   AgeableMob.java (26.1.2), lines 250-252:
//     public static int getSpeedUpSecondsWhenFeeding(final int ticksUntilAdult) {
//        return (int)(ticksUntilAdult / 20 * 0.1F);
//     }
//
// This is a classic Java mixed int/float trap and the only thing ported here:
//   1. `ticksUntilAdult / 20` is INTEGER division (int/int -> int, truncates
//      toward zero), NOT a float divide. So e.g. 39/20 == 1, -39/20 == -1.
//   2. The int quotient is then multiplied by the FLOAT literal 0.1F, so the
//      expression is promoted to `float` and computed in single precision.
//   3. `(int)(...)` narrows the float back to int by truncation toward zero
//      (JLS 5.1.3). For all inputs that fit in int the magnitude of q*0.1F is
//      far inside the float->int range, so the only narrowing rule that bites
//      is round-toward-zero.
//
// To reproduce byte-for-byte in C++ we MUST:
//   * keep `ticksUntilAdult / 20` as int division,
//   * multiply by the `0.1f` FLOAT literal (using the `0.1` double literal would
//     change the product's value before truncation -> a real divergence),
//   * truncate with static_cast<int> on the float result.
//
// Verified bit-for-bit against the REAL AgeableMob.getSpeedUpSecondsWhenFeeding
// (invoked via reflection) by ageable_mob_parity (tools/AgeableMobParity.java).
//
// Related vanilla constants are reproduced for completeness / downstream use;
// they are plain compile-time integers copied verbatim from the source.
#pragma once

namespace mc::world::entity {

// AgeableMob.java (26.1.2) field/constant block, lines 31-36.
inline constexpr int BABY_START_AGE = -24000;        // public static final int
inline constexpr int AGE_LOCK_COOLDOWN_TICKS = 40;   // public static final int
inline constexpr int FORCED_AGE_PARTICLE_TICKS = 40; // private static final int
inline constexpr int DEFAULT_AGE = 0;                // protected static final int
inline constexpr int DEFAULT_FORCED_AGE = 0;         // protected static final int

// AgeableMob.java (26.1.2), lines 250-252. Verbatim static body.
//
//   public static int getSpeedUpSecondsWhenFeeding(final int ticksUntilAdult) {
//      return (int)(ticksUntilAdult / 20 * 0.1F);
//   }
inline int getSpeedUpSecondsWhenFeeding(int ticksUntilAdult) {
    // int division first, then * float 0.1F, then truncate float -> int.
    return static_cast<int>(ticksUntilAdult / 20 * 0.1f);
}

} // namespace mc::world::entity
