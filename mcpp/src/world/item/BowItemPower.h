#pragma once

// 1:1 port of the pure charge-power helper net.minecraft.world.item.BowItem
// (26.1.2). The whole rest of BowItem is world/entity/registry-coupled
// (releaseUsing pulls the projectile registry, the player inventory, the level,
// EnchantmentHelper, ...). getPowerForTime is the ONE self-contained static math
// helper: int → float, no world/level/entity/registry/GL access. It maps the
// number of ticks the bow has been drawn to the [0,1] velocity scalar that
// AbstractArrow.shootFromRotation multiplies by 3.0 to get arrow speed.
//
// Java source (BowItem.java:74-82):
//   public static float getPowerForTime(final int timeHeld) {
//      float pow = timeHeld / 20.0F;
//      pow = (pow * pow + pow * 2.0F) / 3.0F;
//      if (pow > 1.0F) {
//         pow = 1.0F;
//      }
//      return pow;
//   }
//
// 1:1 traps that this gate pins down (all in IEEE-754 binary32):
//   * `timeHeld / 20.0F` is an int→float widening division, NOT integer division
//     — a C++ port that wrote `timeHeld / 20` (int) or used a double divisor
//     would diverge. The dividend is the *int* timeHeld widened to float; for
//     |timeHeld| <= 2^24 that widening is exact, but the /20.0F result is rounded
//     to float, and the rest of the expression compounds that rounding.
//   * The quadratic `(pow*pow + pow*2.0F) / 3.0F` must be evaluated entirely in
//     float (single) precision with this exact association: two float multiplies,
//     a float add, then a float divide. Promoting any intermediate to double (the
//     classic C++ `pow*pow` → double trap if a double literal sneaks in) changes
//     the rounding and breaks the gate.
//   * The clamp is a one-sided `if (pow > 1.0F) pow = 1.0F;` — there is NO lower
//     clamp, so negative timeHeld yields a positive pow (the quadratic) that this
//     gate also exercises. `>` (strict) means pow == 1.0F is left untouched.
//
// Every value/operation/constant below is VERBATIM from BowItem.java — nothing is
// invented, tuned, or simplified.

#include <cstdint>

namespace mc::item {

// BowItem.java:74-82 — public static float getPowerForTime(final int timeHeld)
inline float getPowerForTime(std::int32_t timeHeld) {
    float pow = static_cast<float>(timeHeld) / 20.0F;       // :75  int→float, then float divide
    pow = (pow * pow + pow * 2.0F) / 3.0F;                  // :76  all-float quadratic
    if (pow > 1.0F) {                                       // :77  one-sided upper clamp
        pow = 1.0F;                                         // :78
    }
    return pow;                                             // :81
}

} // namespace mc::item
