// 1:1 port of net.minecraft.world.food.FoodConstants (Minecraft 26.1.2).
//
// A pure data/constants class: the hunger/saturation/exhaustion magic numbers used
// throughout the food model, plus one pure helper, saturationByModifier. Every value
// here is copied VERBATIM from 26.1.2/src/net/minecraft/world/food/FoodConstants.java
// (lines 4-32) — no invention, no rounding. Float constants are the exact IEEE-754
// literals written in the Java source (e.g. 0.1F is the float nearest to 0.1, NOT the
// double 0.1).
//
// Parity gate: world/food/FoodConstantsParityTest.cpp vs tools/FoodConstantsParity.java
// (which reads the values reflectively out of the real class, so the ground truth is the
// genuine jar).

#pragma once

namespace mc::food {

struct FoodConstants {
    // ---- integer constants (FoodConstants.java:4,9-13) ----
    static constexpr int MAX_FOOD = 20;
    static constexpr int HEALTH_TICK_COUNT = 80;
    static constexpr int HEALTH_TICK_COUNT_SATURATED = 10;
    static constexpr int HEAL_LEVEL = 18;
    static constexpr int SPRINT_LEVEL = 6;
    static constexpr int STARVE_LEVEL = 0;

    // ---- float constants (FoodConstants.java:5-8,14-28) ----
    // Written exactly as the Java float literals; the C++ `f` suffix yields the same
    // float-rounded value as Java's `F` suffix.
    static constexpr float MAX_SATURATION = 20.0f;
    static constexpr float START_SATURATION = 5.0f;
    static constexpr float SATURATION_FLOOR = 2.5f;
    static constexpr float EXHAUSTION_DROP = 4.0f;

    static constexpr float FOOD_SATURATION_POOR = 0.1f;
    static constexpr float FOOD_SATURATION_LOW = 0.3f;
    static constexpr float FOOD_SATURATION_NORMAL = 0.6f;
    static constexpr float FOOD_SATURATION_GOOD = 0.8f;
    static constexpr float FOOD_SATURATION_MAX = 1.0f;
    static constexpr float FOOD_SATURATION_SUPERNATURAL = 1.2f;

    static constexpr float EXHAUSTION_HEAL = 6.0f;
    static constexpr float EXHAUSTION_JUMP = 0.05f;
    static constexpr float EXHAUSTION_SPRINT_JUMP = 0.2f;
    static constexpr float EXHAUSTION_MINE = 0.005f;
    static constexpr float EXHAUSTION_ATTACK = 0.1f;
    static constexpr float EXHAUSTION_WALK = 0.0f;
    static constexpr float EXHAUSTION_CROUCH = 0.0f;
    static constexpr float EXHAUSTION_SPRINT = 0.1f;
    static constexpr float EXHAUSTION_SWIM = 0.01f;

    // FoodConstants.java:30-32 — pure function, no state. Java performs the multiply
    // chain in float arithmetic (nutrition is widened int->float, all multiplies float).
    static float saturationByModifier(int nutrition, float modifier) {
        return static_cast<float>(nutrition) * modifier * 2.0f;
    }
};

}  // namespace mc::food
